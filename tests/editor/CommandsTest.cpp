#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "editor/commands/AddObjectCommand.h"
#include "editor/commands/MoveObjectsCommand.h"
#include "editor/commands/ResizeObjectCommand.h"

#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;

namespace
{

// Same objects, in the same order, with the same ids and properties.
bool sameScreen(const Screen& a, const Screen& b)
{
    if (a.objectCount() != b.objectCount())
        return false;
    for (std::size_t i = 0; i < a.objectCount(); ++i)
    {
        const HmiObject& x = *a.objects()[i];
        const HmiObject& y = *b.objects()[i];
        if (x.id() != y.id() || !x.hasSameProperties(y))
            return false;
    }
    return true;
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
};

QTEST_MAIN(CommandsTest)
#include "CommandsTest.moc"
