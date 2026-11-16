// PathTool: placing polylines, polygons and pipes point by point, with mouse
// and with touch.

#include "editor/tools/PathTool.h"

#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/objects/Pipe.h"
#include "core/objects/Polygon.h"
#include "core/objects/Polyline.h"
#include "core/property/Prop.h"
#include "editor/FakeCanvas.h"
#include "editor/GestureFactory.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"
#include "editor/tools/ToolController.h"

#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using Kind = PointerGesture::Kind;

class PathToolTest : public QObject
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
    std::unique_ptr<PathTool> m_tool;
    test::GestureFactory m_gestures;
    int m_selectRequests = 0;

    void tap(QPointF p) { m_tool->handleGesture(m_gestures.tap(p)); }
    void doubleTap(QPointF p) { m_tool->handleGesture(m_gestures.make(Kind::DoubleTap, p, p)); }

    [[nodiscard]] const HmiObject* added() const
    {
        return m_screen->objectCount() ? m_screen->objects().back().get() : nullptr;
    }

private Q_SLOTS:
    void init()
    {
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(std::make_unique<Screen>(QStringLiteral("S")));
        m_canvas.reset();
        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_context->setCanvas(&m_canvas);
        m_context->setSnapEnabled(false);
        m_selectRequests = 0;
        m_context->setSelectToolRequest([this] { ++m_selectRequests; });
        m_tool = std::make_unique<PathTool>(*m_context);
        m_gestures = {};
    }

    void cleanup()
    {
        m_tool.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void pipeRoutesAtRightAngles_data() { test::addDeviceRows(); }
    void pipeRoutesAtRightAngles()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_tool->setType(Pipe::kTypeId);
        tap({10, 10});
        tap({100, 14}); // mostly right: stays level
        tap({104, 80}); // mostly down: stays above the last point
        QCOMPARE(m_canvas.path, (std::vector<QPointF>{{10, 10}, {100, 10}, {100, 80}}));
        doubleTap({104, 80});

        const HmiObject* pipe = added();
        QVERIFY(pipe);
        QCOMPARE(pipe->typeId(), Pipe::kTypeId);
        QCOMPARE(pipe->geometry(), QRectF(10, 10, 90, 70));
        QCOMPARE(pipe->value<PointList>(Prop::Points),
                 (PointList{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}));
        QCOMPARE(m_selection.ids(), std::vector{pipe->id()});
        QCOMPARE(m_selectRequests, 1);
        QVERIFY(m_canvas.path.empty());
        QCOMPARE(m_stack.count(), 1);
        m_stack.undo();
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
    }

    void altAllowsAnyAngle()
    {
        m_tool->setType(Pipe::kTypeId);
        tap({0, 0});
        m_gestures.modifiers = Qt::AltModifier;
        tap({50, 20});
        QCOMPARE(m_tool->points().back(), QPointF(50, 20));
    }

    void dragShowsAndAddsTheNextSegment_data() { test::addDeviceRows(); }
    void dragShowsAndAddsTheNextSegment()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_tool->setType(Polyline::kTypeId);
        const auto sequence = m_gestures.drag({0, 0}, {60, 40});
        for (std::size_t i = 0; i + 1 < sequence.size(); ++i)
            m_tool->handleGesture(sequence[i]);
        QCOMPARE(m_canvas.path, (std::vector<QPointF>{{0, 0}, {60, 40}}));
        m_tool->handleGesture(sequence.back());
        QCOMPARE(m_tool->points(), (std::vector<QPointF>{{0, 0}, {60, 40}}));
        QVERIFY(m_tool->keyPress(Qt::Key_Return, {}));
        QCOMPARE(added()->geometry(), QRectF(0, 0, 60, 40));
    }

    void tooFewPointsPlaceNothing()
    {
        m_tool->setType(Polygon::kTypeId); // needs three points
        tap({0, 0});
        tap({50, 0});
        doubleTap({50, 0});
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
        QVERIFY(m_tool->points().empty());
        QCOMPARE(m_stack.count(), 0);
    }

    void keysEditAndCancel()
    {
        m_tool->setType(Polyline::kTypeId);
        tap({0, 0});
        tap({10, 10});
        QVERIFY(m_tool->keyPress(Qt::Key_Backspace, {}));
        QCOMPARE(m_tool->points().size(), std::size_t(1));
        QVERIFY(m_tool->keyPress(Qt::Key_Escape, {}));
        QVERIFY(m_tool->points().empty());
        QCOMPARE(m_selectRequests, 0);
        QVERIFY(m_tool->keyPress(Qt::Key_Escape, {})); // nothing left: back to select
        QCOMPARE(m_selectRequests, 1);
    }

    void pointsSnapToTheGrid()
    {
        m_context->setSnapEnabled(true);
        m_context->setSnapSettings({.toGrid = true, .toObjects = false, .gridSpacing = 10.0});
        m_tool->setType(Polyline::kTypeId);
        tap({3, 4});
        tap({58, 33});
        QCOMPARE(m_tool->points(), (std::vector<QPointF>{{0, 0}, {60, 30}}));
    }

    void controllerPicksThePathTool()
    {
        ToolController controller(*m_context);
        controller.createObjects(Pipe::kTypeId);
        QCOMPARE(controller.tool(), ToolController::Tool::Path);
        QCOMPARE(controller.createType(), Pipe::kTypeId);
        controller.createObjects(ObjectTypeId{"rectangle"});
        QCOMPARE(controller.tool(), ToolController::Tool::Create);
    }
};

QTEST_MAIN(PathToolTest)
#include "PathToolTest.moc"
