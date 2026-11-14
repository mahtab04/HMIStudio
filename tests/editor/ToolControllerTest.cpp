#include "editor/tools/ToolController.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Rectangle.h"
#include "editor/FakeCanvas.h"
#include "editor/GestureFactory.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using Kind = PointerGesture::Kind;
using Tool = ToolController::Tool;

class ToolControllerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    QUndoStack m_stack;
    SelectionModel m_selection;
    InputSettings m_input;
    test::FakeCanvas m_canvas;
    std::unique_ptr<ToolContext> m_context;
    std::unique_ptr<ToolController> m_controller;
    test::GestureFactory m_gestures;

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {10, 10, 100, 50});
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());
        m_canvas.reset();
        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_context->setCanvas(&m_canvas);
        m_controller = std::make_unique<ToolController>(*m_context);
        m_gestures = {};
    }

    void cleanup()
    {
        m_controller.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void pinchZoomsAndPansWithAnyTool()
    {
        for (const Tool tool : {Tool::Select, Tool::Pan, Tool::Create})
        {
            m_controller->setTool(tool);
            m_canvas.reset();
            PointerGesture g = m_gestures.make(Kind::PinchUpdate, {100, 100}, {110, 100}, {10, 0});
            g.device = PointerDevice::Touch;
            g.scaleDelta = 1.5;
            m_controller->handleGesture(g);
            QCOMPARE(m_canvas.zoomValue, 1.5);
            QCOMPARE(m_canvas.pan, QPointF(10, 0));
        }
        QCOMPARE(m_screen->objectCount(), std::size_t(1));
    }

    void wheelZoomsWithControlAndPansOtherwise()
    {
        PointerGesture g = m_gestures.make(Kind::Wheel, {50, 50}, {50, 50});
        g.angleDelta = QPoint(0, 120);
        m_controller->handleGesture(g);
        QCOMPARE(m_canvas.zoomCalls, 0);
        QCOMPARE(m_canvas.pan, QPointF(0, 60));

        g.modifiers = Qt::ControlModifier;
        m_controller->handleGesture(g);
        QCOMPARE(m_canvas.zoomCalls, 1);
        QVERIFY(m_canvas.zoomValue > 1.15 && m_canvas.zoomValue < 1.25);
    }

    void middleButtonPanUpdates()
    {
        m_controller->handleGesture(m_gestures.make(Kind::PanBegin, {0, 0}, {0, 0}));
        m_controller->handleGesture(m_gestures.make(Kind::PanUpdate, {0, 0}, {5, 7}, {5, 7}));
        m_controller->handleGesture(m_gestures.make(Kind::PanEnd, {0, 0}, {5, 7}));
        QCOMPARE(m_canvas.pan, QPointF(5, 7));
    }

    void panToolDragPans_data() { test::addDeviceRows(); }
    void panToolDragPans()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_controller->setTool(Tool::Pan);
        QCOMPARE(m_canvas.cursor, Qt::OpenHandCursor);
        // Dragging across an object pans; it does not move the object.
        for (const PointerGesture& g : m_gestures.drag({50, 30}, {80, 50}))
            m_controller->handleGesture(g);
        QCOMPARE(m_canvas.pan, QPointF(30, 20));
        QCOMPARE(m_screen->objects().front()->geometry().topLeft(), QPointF(10, 10));
    }

    void createToolReturnsToSelectAfterPlacing()
    {
        QSignalSpy changed(m_controller.get(), &ToolController::toolChanged);
        m_controller->createObjects(Rectangle::kTypeId);
        QCOMPARE(m_controller->tool(), Tool::Create);
        QCOMPARE(m_canvas.cursor, Qt::CrossCursor);
        m_controller->handleGesture(m_gestures.tap({400, 300}));
        QCOMPARE(m_controller->tool(), Tool::Select);
        QCOMPARE(m_screen->objectCount(), std::size_t(2));
        QCOMPARE(changed.count(), 2);
    }

    void switchingToolsCancelsPreview()
    {
        const auto sequence = m_gestures.drag({50, 30}, {90, 30});
        m_controller->handleGesture(sequence[0]);
        m_controller->handleGesture(sequence[1]);
        QVERIFY(!m_canvas.preview.isEmpty());
        m_controller->setTool(Tool::Pan);
        QVERIFY(m_canvas.preview.isEmpty());
        QCOMPARE(m_stack.count(), 0);
    }

    void keysReachCurrentTool()
    {
        m_controller->createObjects(Rectangle::kTypeId);
        QVERIFY(m_controller->keyPress(Qt::Key_Escape, {}));
        QCOMPARE(m_controller->tool(), Tool::Select);
    }
};

QTEST_MAIN(ToolControllerTest)
#include "ToolControllerTest.moc"
