#include "core/model/GroupObject.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "editor/commands/AddObjectCommand.h"
#include "editor/commands/GroupObjectsCommand.h"
#include "editor/commands/MoveObjectsCommand.h"
#include "editor/commands/RemoveObjectsCommand.h"
#include "editor/commands/ReorderCommand.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/commands/SetPropertyCommand.h"
#include "editor/commands/UngroupCommand.h"

#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;

namespace
{

// Same id and properties, and the same children with the same ids.
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

// Same objects, in the same order, with the same ids and properties.
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

class CommandsTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    ObjectId m_rect;
    ObjectId m_ellipse;
    QUndoStack m_stack;

    // Pushes `command`, then checks that undo restores the model exactly and
    // redo reproduces the state after the first push, twice over.
    void checkRoundTrip(QUndoCommand* command)
    {
        const auto before = m_screen->clone();
        m_stack.push(command);
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
        builder.add(Rectangle::kTypeId, {10, 10, 100, 50});
        m_rect = builder.lastId();
        builder.add(Ellipse::kTypeId, {200, 100, 80, 80});
        m_ellipse = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());
    }

    void cleanup()
    {
        m_stack.clear();
        m_project.reset();
    }

    void addObject()
    {
        const auto original = m_screen->clone();
        auto object = std::make_unique<Rectangle>();
        const ObjectId id = object->id();
        m_stack.push(new AddObjectCommand(*m_project, m_screen->id(), std::move(object)));
        QCOMPARE(m_screen->objectCount(), std::size_t(3));
        QCOMPARE(m_screen->indexOf(id), std::optional<std::size_t>(2));
        const auto after = m_screen->clone();

        m_stack.undo();
        QVERIFY(sameScreen(*m_screen, *original));
        m_stack.redo();
        QVERIFY(sameScreen(*m_screen, *after));
        QVERIFY(m_stack.undoText().contains(u"Rectangle"));
    }

    void addAtIndexKeepsIndexOnRedo()
    {
        m_stack.push(
            new AddObjectCommand(*m_project, m_screen->id(), std::make_unique<Ellipse>(), 0));
        const auto after = m_screen->clone();
        m_stack.undo();
        m_stack.redo();
        QVERIFY(sameScreen(*m_screen, *after));
    }

    void moveObjects()
    {
        const auto original = m_screen->clone();
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect, m_ellipse},
                                            QPointF(15, -5)));
        QCOMPARE(m_screen->object(m_rect)->geometry().topLeft(), QPointF(25, 5));
        QCOMPARE(m_screen->object(m_ellipse)->geometry().topLeft(), QPointF(215, 95));
        const auto after = m_screen->clone();

        m_stack.undo();
        QVERIFY(sameScreen(*m_screen, *original));
        m_stack.redo();
        QVERIFY(sameScreen(*m_screen, *after));
        QCOMPARE(m_stack.undoText(), QStringLiteral("Move 2 object(s)"));
    }

    void resizeObject()
    {
        const auto original = m_screen->clone();
        m_stack.push(new ResizeObjectCommand(*m_project, m_screen->id(), m_rect,
                                             QRectF(10, 10, 100, 50), QRectF(0, 5, 140, 70)));
        QCOMPARE(m_screen->object(m_rect)->geometry(), QRectF(0, 5, 140, 70));
        const auto after = m_screen->clone();

        m_stack.undo();
        QVERIFY(sameScreen(*m_screen, *original));
        m_stack.redo();
        QVERIFY(sameScreen(*m_screen, *after));
    }

    void commandsNotifyTheDocument()
    {
        QSignalSpy changed(&m_project->notifier(), &DocumentNotifier::propertyChanged);
        QSignalSpy added(&m_project->notifier(), &DocumentNotifier::objectAdded);
        QSignalSpy removed(&m_project->notifier(), &DocumentNotifier::objectRemoved);
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, QPointF(1, 1)));
        QCOMPARE(changed.count(), 2);
        m_stack.push(new AddObjectCommand(*m_project, m_screen->id(), std::make_unique<Ellipse>()));
        QCOMPARE(added.count(), 1);
        m_stack.undo();
        QCOMPARE(removed.count(), 1);
    }

    void removeObjects()
    {
        m_stack.push(new AddObjectCommand(*m_project, m_screen->id(), std::make_unique<Ellipse>()));
        const ObjectId top = m_screen->objects().back()->id();
        // Bottom and top object; the middle one stays.
        checkRoundTrip(new RemoveObjectsCommand(*m_project, m_screen->id(), {top, m_rect}));
        QCOMPARE(zOrder(*m_screen), std::vector{m_ellipse});
        QCOMPARE(m_stack.undoText(), QStringLiteral("Delete 2 object(s)"));
    }

    void setPropertyOnSeveralObjects()
    {
        checkRoundTrip(new SetPropertyCommand(*m_project, m_screen->id(), {m_rect, m_ellipse},
                                              Prop::FillColor, QColor(Qt::red)));
        QCOMPARE(m_screen->object(m_rect)->value<QColor>(Prop::FillColor), QColor(Qt::red));
        QCOMPARE(m_screen->object(m_ellipse)->value<QColor>(Prop::FillColor), QColor(Qt::red));
        QVERIFY(m_stack.undoText().contains(u"Fill colour"));
    }

    void reorder()
    {
        checkRoundTrip(new ReorderCommand(*m_project, m_screen->id(), {m_rect, m_ellipse},
                                          {m_ellipse, m_rect}, QStringLiteral("Order")));
        QCOMPARE(zOrder(*m_screen), (std::vector{m_ellipse, m_rect}));
    }

    void groupAndUngroup()
    {
        m_stack.push(new AddObjectCommand(*m_project, m_screen->id(), std::make_unique<Ellipse>()));
        const ObjectId top = m_screen->objects().back()->id();
        const auto original = m_screen->clone();

        // Grouping the bottom and middle objects puts the group in the
        // middle object's place, below the top one.
        auto* group = new GroupObjectsCommand(*m_project, m_screen->id(), {m_ellipse, m_rect});
        const ObjectId groupId = group->groupId();
        checkRoundTrip(group);
        QCOMPARE(zOrder(*m_screen), (std::vector{groupId, top}));
        const HmiObject* g = m_screen->object(groupId);
        QCOMPARE(g->geometry(), QRectF(10, 10, 270, 170));
        QCOMPARE(g->children()[0]->id(), m_rect); // z-order kept inside
        QCOMPARE(g->children()[1]->id(), m_ellipse);

        // Ungrouping gives back the same objects in the same places.
        auto* ungroup = new UngroupCommand(*m_project, m_screen->id(), {groupId});
        checkRoundTrip(ungroup);
        QCOMPARE(ungroup->childIds(), (std::vector{m_rect, m_ellipse}));
        QVERIFY(sameScreen(*m_screen, *original));
    }

    void ungroupAfterMovingTheGroup()
    {
        auto* group = new GroupObjectsCommand(*m_project, m_screen->id(), {m_rect, m_ellipse});
        const ObjectId groupId = group->groupId();
        m_stack.push(group);
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {groupId}, {5, 7}));
        m_stack.push(new UngroupCommand(*m_project, m_screen->id(), {groupId}));
        QCOMPARE(m_screen->object(m_rect)->geometry(), QRectF(15, 17, 100, 50));
        QCOMPARE(m_screen->object(m_ellipse)->geometry(), QRectF(205, 107, 80, 80));
    }

    void nudgesMergeIntoOneStep()
    {
        using Kind = MoveObjectsCommand::Kind;
        const auto original = m_screen->clone();
        for (int i = 0; i < 3; ++i)
            m_stack.push(
                new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {1, 0}, Kind::Nudge));
        m_stack.push(
            new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {0, 10}, Kind::Nudge));
        QCOMPARE(m_stack.count(), 1);
        QCOMPARE(m_screen->object(m_rect)->geometry().topLeft(), QPointF(13, 20));
        m_stack.undo();
        QVERIFY(sameScreen(*m_screen, *original));

        // A different set of objects starts a new step; drags never merge.
        m_stack.redo();
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect, m_ellipse}, {1, 0},
                                            Kind::Nudge));
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {1, 0}));
        m_stack.push(new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {1, 0}));
        QCOMPARE(m_stack.count(), 4);
    }

    void nudgingBackLeavesNoStep()
    {
        using Kind = MoveObjectsCommand::Kind;
        m_stack.push(
            new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {1, 0}, Kind::Nudge));
        m_stack.push(
            new MoveObjectsCommand(*m_project, m_screen->id(), {m_rect}, {-1, 0}, Kind::Nudge));
        QCOMPARE(m_stack.count(), 0);
        QCOMPARE(m_screen->object(m_rect)->geometry().topLeft(), QPointF(10, 10));
    }
};

QTEST_MAIN(CommandsTest)
#include "CommandsTest.moc"
