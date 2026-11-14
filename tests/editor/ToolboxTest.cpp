#include "core/model/Project.h"
#include "core/objects/Ellipse.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"
#include "editor/tools/ToolContext.h"
#include "quick/CanvasItem.h"

#include <QAbstractItemModelTester>
#include <QQuickWindow>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;

class ToolboxTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    QUndoStack m_stack;
    SelectionModel m_selection;
    InputSettings m_input;
    std::unique_ptr<ToolContext> m_context;
    std::unique_ptr<QQuickWindow> m_window;
    CanvasItem* m_canvas = nullptr;
    std::unique_ptr<ToolboxDragController> m_drag;

private Q_SLOTS:
    void init()
    {
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(
            std::make_unique<Screen>(QStringLiteral("S"), QSizeF(800, 600)));
        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());

        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(848, 648);
        m_canvas = new CanvasItem(m_window->contentItem());
        m_canvas->setSize(QSizeF(848, 648));
        m_canvas->setScreen(m_screen, &m_project->notifier());
        m_context->setCanvas(m_canvas);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));

        m_drag = std::make_unique<ToolboxDragController>(*m_context);
        m_drag->attachCanvas(m_canvas);
    }

    void cleanup()
    {
        m_drag.reset();
        m_window.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void modelListsRegisteredTypes()
    {
        ToolboxModel model;
        QAbstractItemModelTester tester(&model);
        QCOMPARE(model.rowCount(), 4);
        const QModelIndex first = model.index(0);
        QCOMPARE(model.data(first, ToolboxModel::KeyRole).toString(), QStringLiteral("ellipse"));
        QCOMPARE(model.data(first, ToolboxModel::NameRole).toString(), QStringLiteral("Ellipse"));
        QCOMPARE(model.data(first, ToolboxModel::IconSourceRole).toUrl(),
                 QUrl(QStringLiteral("qrc:/icons/ellipse.svg")));
        QCOMPARE(model.data(first, ToolboxModel::CategoryRole).toString(),
                 QStringLiteral("Basic objects"));
        QVERIFY(model.roleNames().values().contains("iconSource"));
    }

    void dropOnCanvasAddsObject()
    {
        // Canvas fits the screen at 100 % with a 24 px margin, so window
        // position (124, 124) is scene (100, 100).
        QCOMPARE(m_canvas->zoom(), 1.0);
        m_drag->setActive(true, QStringLiteral("ellipse"));
        QVERIFY(m_drag->dragging());
        m_drag->move(QPointF(10, 10));
        m_drag->move(QPointF(124, 124));
        m_drag->setActive(false, QStringLiteral("ellipse"));
        QVERIFY(!m_drag->dragging());

        QCOMPARE(m_screen->objectCount(), std::size_t(1));
        const HmiObject& added = *m_screen->objects().front();
        QCOMPARE(added.typeId(), Ellipse::kTypeId);
        QCOMPARE(added.geometry(), QRectF(60, 60, 80, 80));
        QCOMPARE(m_selection.ids(), std::vector{added.id()});
        QCOMPARE(m_stack.count(), 1);
    }

    void dropOutsideCanvasDoesNothing()
    {
        m_drag->setActive(true, QStringLiteral("rectangle"));
        m_drag->move(QPointF(124, 124));
        m_drag->move(QPointF(2000, 2000));
        m_drag->setActive(false, QStringLiteral("rectangle"));
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
        QCOMPARE(m_stack.count(), 0);
    }
};

QTEST_MAIN(ToolboxTest)
#include "ToolboxTest.moc"
