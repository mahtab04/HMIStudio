#include "editor/tools/ToolController.h"

#include "editor/tools/CreateTool.h"
#include "editor/tools/PanTool.h"
#include "editor/tools/SelectTool.h"
#include "editor/tools/ToolContext.h"
#include "quick/ICanvasView.h"

#include <cmath>

namespace hmi
{

ToolController::ToolController(ToolContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_select(std::make_unique<SelectTool>(context))
    , m_pan(std::make_unique<PanTool>(context))
    , m_create(std::make_unique<CreateTool>(context))
{
    m_context.setSelectToolRequest([this] { setTool(Tool::Select); });
}

ToolController::~ToolController()
{
    m_context.setSelectToolRequest({});
}

ITool& ToolController::current() const
{
    switch (m_tool)
    {
    case Tool::Pan:
        return *m_pan;
    case Tool::Create:
        return *m_create;
    case Tool::Select:
        break;
    }
    return *m_select;
}

void ToolController::setTool(Tool tool)
{
    if (tool == m_tool)
        return;
    current().deactivate();
    m_tool = tool;
    current().activate();
    applyCursor();
    Q_EMIT toolChanged();
}

void ToolController::createObjects(ObjectTypeId type)
{
    m_create->setType(type);
    if (m_tool == Tool::Create)
        Q_EMIT toolChanged();
    else
        setTool(Tool::Create);
}

ObjectTypeId ToolController::createType() const
{
    return m_create->type();
}

void ToolController::applyCursor()
{
    if (ICanvasView* canvas = m_context.canvas())
        canvas->setToolCursor(current().cursor());
}

void ToolController::handleGesture(const PointerGesture& gesture)
{
    if (!navigate(gesture))
        current().handleGesture(gesture);
}

bool ToolController::keyPress(int key, Qt::KeyboardModifiers modifiers)
{
    return current().keyPress(key, modifiers);
}

bool ToolController::navigate(const PointerGesture& gesture)
{
    using Kind = PointerGesture::Kind;
    ICanvasView* canvas = m_context.canvas();
    switch (gesture.kind)
    {
    case Kind::PinchBegin:
    case Kind::PinchEnd:
    case Kind::PanBegin:
    case Kind::PanEnd:
        return true;
    case Kind::PinchUpdate:
        if (canvas)
        {
            // Pan first so the content follows the fingers, then zoom about
            // the new centre between them.
            canvas->panBy(gesture.viewDelta);
            canvas->zoomAt(gesture.viewPos, gesture.scaleDelta);
        }
        return true;
    case Kind::PanUpdate:
        if (canvas)
            canvas->panBy(gesture.viewDelta);
        return true;
    case Kind::Wheel:
        if (canvas)
        {
            const QPoint a = gesture.angleDelta;
            if (gesture.modifiers.testFlag(Qt::ControlModifier))
                canvas->zoomAt(gesture.viewPos, std::pow(kWheelZoomBase, a.y()));
            else if (gesture.modifiers.testFlag(Qt::ShiftModifier))
                canvas->panBy(QPointF(a.y(), a.x()) * kWheelPanFactor);
            else
                canvas->panBy(QPointF(a.x(), a.y()) * kWheelPanFactor);
        }
        return true;
    default:
        return false;
    }
}

} // namespace hmi
