#include "editor/tools/PanTool.h"

#include "editor/tools/ToolContext.h"
#include "quick/ICanvasView.h"

namespace hmi
{

PanTool::PanTool(ToolContext& context)
    : m_context(context)
{
}

void PanTool::handleGesture(const PointerGesture& gesture)
{
    ICanvasView* canvas = m_context.canvas();
    if (!canvas)
        return;
    switch (gesture.kind)
    {
    case PointerGesture::Kind::DragBegin:
        canvas->setToolCursor(Qt::ClosedHandCursor);
        break;
    case PointerGesture::Kind::DragUpdate:
        canvas->panBy(gesture.viewDelta);
        break;
    case PointerGesture::Kind::DragEnd:
    case PointerGesture::Kind::DragCancel:
        canvas->setToolCursor(cursor());
        break;
    default:
        break;
    }
}

} // namespace hmi
