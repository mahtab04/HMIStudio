// Every editing operation: its effect, and that undo restores the model
// exactly and redo reproduces it (PLAN.md phase 3, "done when").

#include "editor/app/EditActions.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QTest>
#include <QUndoStack>

#include <functional>
#include <memory>

using namespace hmi;

namespace
{

bool sameObject(const HmiObject& x, const HmiObject& y)
{
    if (x.id() != y.id() || !x.hasSameProperties(y))
        return false;
    for (std::size_t i = 0; i < x.children().size(); ++i)
    {
        if (!sameObject(*x.children()[i], *y.children()[i]))
            return false;
    }
    return true;
}

bool sameScreen(const Screen& a, const Screen& b)
{
    if (a.objectCount() != b.objectCount())
        return false;
    for (std::size_t i = 0; i < a.objectCount(); ++i)
    {
        if (!sameObject(*a.objects()[i], *b.objects()[i]))
            return false;
    }
    return true;
}

std::vector<ObjectId> zOrder(const Screen& screen)
{
    std::vector<ObjectId> ids;
    for (const auto& object : screen.objects())
        ids.push_back(object->id());
    return ids;
}

} // namespace

class EditActionsTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    QUndoStack m_stack;
    SelectionModel m_selection;
    InputSettings m_input;
    std::unique_ptr<ToolContext> m_context;
    std::unique_ptr<EditActions> m_actions;
    ObjectId m_a;      // (10, 10, 100, 50)
    ObjectId m_b;      // (200, 40, 80, 80)
    ObjectId m_c;      // (400, 100, 60, 30)
    ObjectId m_locked; // (10, 300, 50, 50), locked

    [[nodiscard]] QRectF geometryOf(ObjectId id) const { return m_screen->object(id)->geometry(); }

    // Runs `operation` on `selection`, expects it to do something, and
    // checks undo and redo twice over.
    void checkUndoable(std::vector<ObjectId> selection, const std::function<bool()>& operation)
    {
        m_selection.set(std::move(selection));
        const auto before = m_screen->clone();
        const int steps = m_stack.count();
        QVERIFY(operation());
        QCOMPARE(m_stack.count(), steps + 1);
        const auto after = m_screen->clone();
        QVERIFY(!sameScreen(*before, *after));
        for (int i = 0; i < 2; ++i)
        {
            m_stack.undo();
            QVERIFY(sameScreen(*m_screen, *before));
            m_stack.redo();
            QVERIFY(sameScreen(*m_screen, *after));
        }
    }

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.size({800, 600});
        builder.add(Rectangle::kTypeId, {10, 10, 100, 50});
        m_a = builder.lastId();
        builder.add(Ellipse::kTypeId, {200, 40, 80, 80});
        m_b = builder.lastId();
        builder.add(Rectangle::kTypeId, {400, 100, 60, 30});
        m_c = builder.lastId();
        builder.add(Rectangle::kTypeId, {10, 300, 50, 50}).with(Prop::Locked, true);
        m_locked = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());

        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_actions = std::make_unique<EditActions>(*m_context);
        QGuiApplication::clipboard()->clear();
    }

    void cleanup()
    {
        m_actions.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void selectAll()
    {
        m_actions->selectAll();
        QCOMPARE(m_selection.size(), std::size_t(4));
    }

    void deleteSelection()
    {
        checkUndoable({m_b, m_locked}, [this] { return m_actions->deleteSelection(); });
        QCOMPARE(zOrder(*m_screen), (std::vector{m_a, m_c}));
        QVERIFY(m_selection.isEmpty());
        m_selection.clear();
        QVERIFY(!m_actions->deleteSelection());
    }

    void duplicate()
    {
        checkUndoable({m_a, m_b}, [this] { return m_actions->duplicate(); });
        QCOMPARE(m_screen->objectCount(), std::size_t(6));
        // The copies are on top, offset, selected and have new ids.
        const auto& copies = m_selection.ids();
        QCOMPARE(copies.size(), std::size_t(2));
        QCOMPARE(geometryOf(copies[0]), QRectF(26, 26, 100, 50));
        QCOMPARE(geometryOf(copies[1]), QRectF(216, 56, 80, 80));
        QVERIFY(copies[0] != m_a);
        QCOMPARE(zOrder(*m_screen).back(), copies[1]);
        QCOMPARE(m_stack.undoText(), QStringLiteral("Duplicate"));
    }

    void copyAndPasteRepeatedly()
    {
        QVERIFY(!m_actions->canPaste());
        m_selection.set({m_a});
        QVERIFY(m_actions->copy());
        QVERIFY(m_actions->canPaste());
        QVERIFY(QGuiApplication::clipboard()->mimeData()->hasFormat(
            QString::fromLatin1(EditActions::kMimeType)));

        checkUndoable({}, [this] { return m_actions->paste(); });
        const ObjectId first = m_selection.ids().front();
        QCOMPARE(geometryOf(first), QRectF(26, 26, 100, 50));
        QVERIFY(first != m_a);

        // Each further paste moves on, so copies do not stack up unseen.
        QVERIFY(m_actions->paste());
        const ObjectId second = m_selection.ids().front();
        QCOMPARE(geometryOf(second), QRectF(42, 42, 100, 50));
        QVERIFY(second != first);
    }

    void cutPastesInPlaceFirst()
    {
        checkUndoable({m_c}, [this] { return m_actions->cut(); });
        QVERIFY(!m_screen->object(m_c));
        QVERIFY(m_actions->paste());
        QCOMPARE(geometryOf(m_selection.ids().front()), QRectF(400, 100, 60, 30));
        QVERIFY(m_actions->paste());
        QCOMPARE(geometryOf(m_selection.ids().front()), QRectF(416, 116, 60, 30));
    }

    void pasteKeepsGroups()
    {
        m_selection.set({m_a, m_b});
        QVERIFY(m_actions->group());
        QVERIFY(m_actions->copy());
        QVERIFY(m_actions->paste());
        const HmiObject* pasted = m_screen->object(m_selection.ids().front());
        QCOMPARE(pasted->children().size(), std::size_t(2));
        QVERIFY(pasted->children()[0]->id() != m_a);
    }

    void nudgeSkipsLockedAndMerges()
    {
        checkUndoable({m_a, m_locked}, [this] { return m_actions->nudge({1, 0}); });
        QVERIFY(m_actions->nudge({0, 10}));
        QCOMPARE(m_stack.count(), 1);
        QCOMPARE(geometryOf(m_a), QRectF(11, 20, 100, 50));
        QCOMPARE(geometryOf(m_locked), QRectF(10, 300, 50, 50));
        m_selection.set({m_locked});
        QVERIFY(!m_actions->nudge({1, 0}));
    }

    void groupAndUngroup()
    {
        QVERIFY(!m_actions->canGroup());
        m_selection.set({m_a});
        QVERIFY(!m_actions->canGroup());

        checkUndoable({m_a, m_c}, [this] { return m_actions->group(); });
        const ObjectId group = m_selection.ids().front();
        QCOMPARE(m_selection.size(), std::size_t(1));
        QCOMPARE(geometryOf(group), QRectF(10, 10, 450, 120));
        QCOMPARE(zOrder(*m_screen), (std::vector{m_b, group, m_locked}));
        QVERIFY(m_actions->canUngroup());

        checkUndoable({group, m_b}, [this] { return m_actions->ungroup(); });
        QCOMPARE(m_selection.ids(), (std::vector{m_b, m_a, m_c}));
        QCOMPARE(zOrder(*m_screen), (std::vector{m_b, m_a, m_c, m_locked}));
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 100, 50));
        QVERIFY(!m_actions->canUngroup());
    }

    void alignOnSelection_data()
    {
        QTest::addColumn<EditActions::Alignment>("alignment");
        QTest::addColumn<QPointF>("a");
        QTest::addColumn<QPointF>("b");
        using A = EditActions::Alignment;
        // Selection bounds: (10, 10) to (280, 120).
        QTest::newRow("left") << A::Left << QPointF(10, 10) << QPointF(10, 40);
        QTest::newRow("centre") << A::HorizontalCentre << QPointF(95, 10) << QPointF(105, 40);
        QTest::newRow("right") << A::Right << QPointF(180, 10) << QPointF(200, 40);
        QTest::newRow("top") << A::Top << QPointF(10, 10) << QPointF(200, 10);
        QTest::newRow("middle") << A::VerticalCentre << QPointF(10, 40) << QPointF(200, 25);
        QTest::newRow("bottom") << A::Bottom << QPointF(10, 70) << QPointF(200, 40);
    }
    void alignOnSelection()
    {
        QFETCH(EditActions::Alignment, alignment);
        QFETCH(QPointF, a);
        QFETCH(QPointF, b);
        checkUndoable({m_a, m_b}, [&] { return m_actions->align(alignment); });
        QCOMPARE(geometryOf(m_a).topLeft(), a);
        QCOMPARE(geometryOf(m_b).topLeft(), b);
    }

    void alignSingleObjectOnScreen()
    {
        checkUndoable({m_c}, [this]
                      { return m_actions->align(EditActions::Alignment::HorizontalCentre); });
        QCOMPARE(geometryOf(m_c).topLeft(), QPointF(370, 100));
        // Already there: nothing to do.
        QVERIFY(!m_actions->align(EditActions::Alignment::HorizontalCentre));
    }

    void lockedObjectsKeepTheirPlaceWhenAligning()
    {
        checkUndoable({m_a, m_locked},
                      [this] { return m_actions->align(EditActions::Alignment::Bottom); });
        QCOMPARE(geometryOf(m_locked), QRectF(10, 300, 50, 50));
        QCOMPARE(geometryOf(m_a).bottom(), 350.0);
    }

    void distribute()
    {
        QVERIFY(!m_actions->canDistribute());
        // Widths 100 + 80 + 60 over 10..460: gaps of 105.
        checkUndoable({m_c, m_a, m_b}, [this] { return m_actions->distribute(Qt::Horizontal); });
        QCOMPARE(geometryOf(m_a).x(), 10.0);
        QCOMPARE(geometryOf(m_b).x(), 215.0);
        QCOMPARE(geometryOf(m_c).x(), 400.0);

        // Vertically they already overlap by the same 20 each: nothing to do.
        QVERIFY(!m_actions->distribute(Qt::Vertical));
    }

    void matchSize()
    {
        checkUndoable({m_b, m_a, m_c},
                      [this] { return m_actions->matchSize(EditActions::SizeMatch::Both); });
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 80, 80));
        QCOMPARE(geometryOf(m_c), QRectF(400, 100, 80, 80));
        // The locked object keeps its size, so there is nothing to change.
        m_selection.set({m_a, m_locked});
        QVERIFY(!m_actions->matchSize(EditActions::SizeMatch::Width));
    }

    void reorder_data()
    {
        QTest::addColumn<EditActions::Order>("order");
        QTest::addColumn<QList<int>>("expected"); // indices into a, b, c, locked
        using O = EditActions::Order;
        QTest::newRow("forward") << O::Forward << QList<int>{1, 0, 3, 2};
        QTest::newRow("backward") << O::Backward << QList<int>{0, 2, 1, 3};
        QTest::newRow("front") << O::ToFront << QList<int>{1, 3, 0, 2};
        QTest::newRow("back") << O::ToBack << QList<int>{0, 2, 1, 3};
    }
    void reorder()
    {
        QFETCH(EditActions::Order, order);
        QFETCH(QList<int>, expected);
        const std::vector<ObjectId> objects{m_a, m_b, m_c, m_locked};
        // A and C selected, B between them.
        checkUndoable({m_a, m_c}, [&] { return m_actions->reorder(order); });
        std::vector<ObjectId> want;
        for (int i : expected)
            want.push_back(objects[std::size_t(i)]);
        QCOMPARE(zOrder(*m_screen), want);
    }

    void reorderAtTheEdgeDoesNothing()
    {
        m_selection.set({m_locked});
        QVERIFY(!m_actions->reorder(EditActions::Order::Forward));
        QVERIFY(!m_actions->reorder(EditActions::Order::ToFront));
        QCOMPARE(m_stack.count(), 0);
    }
};

QTEST_MAIN(EditActionsTest)
#include "EditActionsTest.moc"
