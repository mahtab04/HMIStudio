#include "editor/tools/CreateTool.h"

#include "core/model/Project.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
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

class CreateToolTest : public QObject
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
    std::unique_ptr<CreateTool> m_tool;
    test::GestureFactory m_gestures;
    int m_selectToolRequests = 0;

    void run(const std::vector<PointerGesture>& sequence)
    {
        for (const PointerGesture& g : sequence)
            m_tool->handleGesture(g);
    }

    [[nodiscard]] const HmiObject* newest() const
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
        m_selectToolRequests = 0;
        m_context->setSelectToolRequest([this] { ++m_selectToolRequests; });
        m_tool = std::make_unique<CreateTool>(*m_context);
        m_tool->setType(Rectangle::kTypeId);
        m_gestures = {};
    }

    void cleanup()
    {
        m_tool.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void tapPlacesDefaultSizeCentred_data() { test::addDeviceRows(); }
    void tapPlacesDefaultSizeCentred()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        m_tool->handleGesture(m_gestures.tap({300, 200}));

        QCOMPARE(m_screen->objectCount(), std::size_t(1));
        QCOMPARE(newest()->typeId(), Rectangle::kTypeId);
        QCOMPARE(newest()->geometry(), QRectF(250, 170, 100, 60));
        QCOMPARE(m_selection.ids(), std::vector{newest()->id()});
        QCOMPARE(m_selectToolRequests, 1);
        QCOMPARE(m_stack.count(), 1);

        m_stack.undo();
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
    }

    void dragPlacesInRectangle_data() { test::addDeviceRows(); }
    void dragPlacesInRectangle()
    {
        QFETCH(PointerDevice, device);
        m_gestures.device = device;
        const auto sequence = m_gestures.drag({100, 100}, {40.4, 160.6});
        for (std::size_t i = 0; i + 1 < sequence.size(); ++i)
            m_tool->handleGesture(sequence[i]);
        QVERIFY(m_canvas.rubberBand);
        QCOMPARE(m_screen->objectCount(), std::size_t(0));

        m_tool->handleGesture(sequence.back());
        QVERIFY(!m_canvas.rubberBand);
        QCOMPARE(newest()->geometry(), QRectF(40, 100, 60, 61));
    }

    void tinyDragCountsAsTap()
    {
        run(m_gestures.drag({300, 200}, {302, 201}));
        QCOMPARE(newest()->geometry().size(), QSizeF(100, 60));
    }

    void lineDirectionFollowsDrag()
    {
        m_tool->setType(Line::kTypeId);
        run(m_gestures.drag({0, 100}, {100, 0}));
        QCOMPARE(newest()->value<EnumValue>(Prop::LineDirection), EnumValue{Line::Rising});
        run(m_gestures.drag({0, 0}, {100, 100}));
        QCOMPARE(newest()->value<EnumValue>(Prop::LineDirection), EnumValue{Line::Falling});
    }

    void cancelPlacesNothing()
    {
        auto sequence = m_gestures.drag({0, 0}, {100, 100});
        sequence.back().kind = Kind::DragCancel;
        run(sequence);
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
        QVERIFY(!m_canvas.rubberBand);
        QCOMPARE(m_selectToolRequests, 0);
    }

    void escapeReturnsToSelectTool()
    {
        QVERIFY(m_tool->keyPress(Qt::Key_Escape, {}));
        QCOMPARE(m_selectToolRequests, 1);
        QCOMPARE(m_screen->objectCount(), std::size_t(0));
    }
};

QTEST_MAIN(CreateToolTest)
#include "CreateToolTest.moc"
