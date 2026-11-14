// SelectTool driven by synthetic gestures, each test once as mouse and once
// as touch where the behaviour should match.

#include "editor/tools/SelectTool.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "editor/FakeCanvas.h"
#include "editor/GestureFactory.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using Kind = PointerGesture::Kind;

class SelectToolTest : public QObject
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
    std::unique_ptr<SelectTool> m_tool;
    ObjectId m_a;      // (10, 10, 100, 50)
    ObjectId m_b;      // (200, 10, 100, 50)
    ObjectId m_locked; // (10, 200, 100, 50)
    test::GestureFactory m_gestures;

    void run(const std::vector<PointerGesture>& sequence)
    {
        for (const PointerGesture& g : sequence)
            m_tool->handleGesture(g);
    }

    [[nodiscard]] QRectF geometryOf(ObjectId id) const { return m_screen->object(id)->geometry(); }

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {10, 10, 100, 50});
        m_a = builder.lastId();
        builder.add(Rectangle::kTypeId, {200, 10, 100, 50});
        m_b = builder.lastId();
        builder.add(Rectangle::kTypeId, {10, 200, 100, 50}).with(Prop::Locked, true);
        m_locked = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());

        m_canvas.reset();
        m_selection.clear();
        m_selection.setMultiSelectMode(false);
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_context->setCanvas(&m_canvas);
        m_tool = std::make_unique<SelectTool>(*m_context);
        m_gestures = {};
    }

    void cleanup()
    {
        m_tool.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void tapSelectsAndClears_data() { test::addDeviceRows(); }
    void tapSelectsAndClears()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;

        m_tool->handleGesture(m_gestures.tap({50, 30}));
        QCOMPARE(m_selection.ids(), std::vector{m_a});
        m_tool->handleGesture(m_gestures.tap({250, 30}));
        QCOMPARE(m_selection.ids(), std::vector{m_b});
        m_tool->handleGesture(m_gestures.tap({500, 500}));
        QVERIFY(m_selection.isEmpty());
    }

    void touchToleranceIsLarger()
    {
        // 8 px outside the rectangle: a miss for the mouse, a hit for a finger.
        m_gestures.device = PointerDevice::Mouse;
        m_tool->handleGesture(m_gestures.tap({118, 30}));
        QVERIFY(m_selection.isEmpty());
        m_gestures.device = PointerDevice::Touch;
        m_tool->handleGesture(m_gestures.tap({118, 30}));
        QCOMPARE(m_selection.ids(), std::vector{m_a});
    }

    void shiftClickAddsAndToggles()
    {
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        m_gestures.modifiers = Qt::ShiftModifier;
        m_tool->handleGesture(m_gestures.tap({250, 30}));
        QCOMPARE(m_selection.ids(), (std::vector{m_a, m_b}));
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        QCOMPARE(m_selection.ids(), std::vector{m_b});
    }

    void multiSelectModeAddsWithoutModifier_data() { test::addDeviceRows(); }
    void multiSelectModeAddsWithoutModifier()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_selection.setMultiSelectMode(true);
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        m_tool->handleGesture(m_gestures.tap({250, 30}));
        QCOMPARE(m_selection.ids(), (std::vector{m_a, m_b}));
        // A tap on empty space keeps the selection in this mode.
        m_tool->handleGesture(m_gestures.tap({500, 500}));
        QCOMPARE(m_selection.size(), std::size_t(2));
    }

    void longPressAddsToSelection()
    {
        m_gestures.device = PointerDevice::Touch;
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        m_tool->handleGesture(m_gestures.longPress({250, 30}));
        QCOMPARE(m_selection.ids(), (std::vector{m_a, m_b}));
    }

    void dragMovesWithPreviewThenOneCommand_data() { test::addDeviceRows(); }
    void dragMovesWithPreviewThenOneCommand()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        const auto sequence = m_gestures.drag({50, 30}, {80, 70});

        for (std::size_t i = 0; i + 1 < sequence.size(); ++i)
            m_tool->handleGesture(sequence[i]);
        // Preview only: the model has not moved and nothing was pushed.
        QCOMPARE(m_canvas.preview.value(m_a), QRectF(40, 50, 100, 50));
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 100, 50));
        QCOMPARE(m_stack.count(), 0);

        m_tool->handleGesture(sequence.back());
        QCOMPARE(geometryOf(m_a), QRectF(40, 50, 100, 50));
        QCOMPARE(m_stack.count(), 1);
        QVERIFY(m_canvas.preview.isEmpty());
        QCOMPARE(m_selection.ids(), std::vector{m_a});

        m_stack.undo();
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 100, 50));
    }

    void dragMovesWholeSelection()
    {
        m_selection.set({m_a, m_b});
        run(m_gestures.drag({250, 30}, {260, 40}));
        QCOMPARE(geometryOf(m_a).topLeft(), QPointF(20, 20));
        QCOMPARE(geometryOf(m_b).topLeft(), QPointF(210, 20));
        QCOMPARE(m_stack.count(), 1);
    }

    void cancelLeavesModelUntouched_data() { test::addDeviceRows(); }
    void cancelLeavesModelUntouched()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        auto sequence = m_gestures.drag({50, 30}, {150, 130});
        sequence.back().kind = Kind::DragCancel;
        run(sequence);
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 100, 50));
        QCOMPARE(m_stack.count(), 0);
        QVERIFY(m_canvas.preview.isEmpty());
    }

    void escapeKeyCancelsOrClears()
    {
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        QVERIFY(m_tool->keyPress(Qt::Key_Escape, {}));
        QVERIFY(m_selection.isEmpty());
    }

    void handleDragResizes_data() { test::addDeviceRows(); }
    void handleDragResizes()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        // Bottom-right handle of (10, 10, 100, 50) is at (110, 60).
        run(m_gestures.drag({112, 62}, {142, 82}));
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 130, 70));
        QCOMPARE(m_stack.count(), 1);
        m_stack.undo();
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 100, 50));
    }

    void touchReachesHandleFromFurtherAway()
    {
        m_tool->handleGesture(m_gestures.tap({50, 30}));
        // 15 px from the handle: outside the mouse's 12 px square, inside the
        // 44 px touch square. With the mouse this starts a move instead.
        m_gestures.device = PointerDevice::Touch;
        run(m_gestures.drag({125, 60}, {135, 60}));
        QCOMPARE(geometryOf(m_a), QRectF(10, 10, 110, 50));
    }

    void lockedObjectsDoNotMove_data() { test::addDeviceRows(); }
    void lockedObjectsDoNotMove()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        run(m_gestures.drag({50, 220}, {90, 260}));
        QCOMPARE(m_selection.ids(), std::vector{m_locked});
        QCOMPARE(geometryOf(m_locked), QRectF(10, 200, 100, 50));
        QCOMPARE(m_stack.count(), 0);
    }

    void mouseRubberBandSelects()
    {
        // Left to right: enclosed objects only.
        run(m_gestures.drag({0, 0}, {150, 100}));
        QCOMPARE(m_selection.ids(), std::vector{m_a});
        QVERIFY(!m_canvas.rubberBand);

        // Right to left, starting on empty space: touched objects too.
        run(m_gestures.drag({320, 40}, {50, 0}));
        QCOMPARE(m_selection.ids(), (std::vector{m_a, m_b}));
    }

    void rubberBandShowsWhileDragging()
    {
        const auto sequence = m_gestures.drag({0, 0}, {150, 100});
        for (std::size_t i = 0; i + 1 < sequence.size(); ++i)
            m_tool->handleGesture(sequence[i]);
        QCOMPARE(m_canvas.rubberBand, std::optional(QRectF(0, 0, 150, 100)));
        m_tool->handleGesture(sequence.back());
        QVERIFY(!m_canvas.rubberBand);
    }

    void touchDragOnEmptySpacePans()
    {
        m_gestures.device = PointerDevice::Touch;
        run(m_gestures.drag({500, 500}, {530, 520}));
        QCOMPARE(m_canvas.pan, QPointF(30, 20));
        QVERIFY(m_selection.isEmpty());
    }

    void touchLongPressThenDragRubberBands()
    {
        m_gestures.device = PointerDevice::Touch;
        run(m_gestures.drag({0, 0}, {150, 100}, 3, true));
        QCOMPARE(m_canvas.pan, QPointF());
        QCOMPARE(m_selection.ids(), std::vector{m_a});
    }
};

QTEST_MAIN(SelectToolTest)
#include "SelectToolTest.moc"
