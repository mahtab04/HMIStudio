#include "editor/tools/CreateTool.h"

#include "core/model/HmiObject.h"
#include "editor/tools/ObjectPlacement.h"
#include "editor/tools/ToolContext.h"
#include "quick/ICanvasView.h"

#include <cmath>

namespace hmi
{

CreateTool::CreateTool(ToolContext& context)
    : m_context(context)
{
}

void CreateTool::deactivate()
{
    if (ICanvasView* canvas = m_context.canvas())
        canvas->setRubberBand(std::nullopt);
    m_dragging = false;
}

void CreateTool::handleGesture(const PointerGesture& gesture)
{
    using Kind = PointerGesture::Kind;
    ICanvasView* canvas = m_context.canvas();
    switch (gesture.kind)
    {
    case Kind::Tap:
    case Kind::DoubleTap:
        place(gesture);
        break;
    case Kind::DragBegin:
        m_dragging = true;
        break;
    case Kind::DragUpdate:
        if (m_dragging && canvas)
            canvas->setRubberBand(QRectF(gesture.startScenePos, gesture.scenePos).normalized());
        break;
    case Kind::DragEnd:
        if (m_dragging)
            place(gesture);
        break;
    case Kind::DragCancel:
        deactivate();
        break;
    default:
        break;
    }
}

bool CreateTool::keyPress(int key, Qt::KeyboardModifiers /*modifiers*/)
{
    if (key != Qt::Key_Escape)
        return false;
    deactivate();
    m_context.requestSelectTool();
    return true;
}

void CreateTool::place(const PointerGesture& gesture)
{
    const QPointF viewSize = gesture.viewPos - gesture.startViewPos;
    const bool isDrag =
        gesture.kind == PointerGesture::Kind::DragEnd &&
        (std::abs(viewSize.x()) >= kMinimumDragSize || std::abs(viewSize.y()) >= kMinimumDragSize);
    deactivate();

    std::unique_ptr<HmiObject> object;
    if (isDrag)
    {
        const QPointF d = gesture.scenePos - gesture.startScenePos;
        // Dragging up-right or down-left draws a rising line.
        const bool rising = (d.x() > 0) != (d.y() > 0);
        object = ObjectPlacement::createInRect(
            m_type, QRectF(gesture.startScenePos, gesture.scenePos), rising);
    }
    else
    {
        object = ObjectPlacement::createCentred(m_type, gesture.scenePos);
    }

    if (ObjectPlacement::add(m_context, std::move(object)))
        m_context.requestSelectTool();
}

} // namespace hmi
