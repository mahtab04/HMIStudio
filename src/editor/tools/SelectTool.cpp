#include "editor/tools/SelectTool.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "core/spatial/HitTester.h"
#include "editor/commands/MoveObjectsCommand.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"
#include "quick/ICanvasView.h"

#include <QUndoStack>

#include <cmath>

namespace hmi
{

namespace
{

QRectF roundedRect(const QRectF& r)
{
    return {std::round(r.x()), std::round(r.y()), std::round(r.width()), std::round(r.height())};
}

} // namespace

SelectTool::SelectTool(ToolContext& context)
    : m_context(context)
{
}

void SelectTool::deactivate()
{
    cancel();
}

void SelectTool::handleGesture(const PointerGesture& gesture)
{
    using Kind = PointerGesture::Kind;
    switch (gesture.kind)
    {
    case Kind::Tap:
    case Kind::DoubleTap:
        tap(gesture);
        break;
    case Kind::LongPress:
        longPress(gesture);
        break;
    case Kind::ContextMenu:
        // The context menu itself arrives in phase 3; make sure the object
        // under the pointer is part of the selection it will act on.
        if (HmiObject* hit = objectAt(gesture); hit && !m_context.selection().contains(hit->id()))
            m_context.selection().selectOnly(hit->id());
        break;
    case Kind::DragBegin:
        dragBegin(gesture);
        break;
    case Kind::DragUpdate:
        dragUpdate(gesture);
        break;
    case Kind::DragEnd:
        dragEnd(gesture);
        break;
    case Kind::DragCancel:
        cancel();
        break;
    default:
        break;
    }
}

bool SelectTool::keyPress(int key, Qt::KeyboardModifiers /*modifiers*/)
{
    if (key != Qt::Key_Escape)
        return false;
    if (m_mode != Mode::None)
        cancel();
    else
        m_context.selection().clear();
    return true;
}

HmiObject* SelectTool::objectAt(const PointerGesture& gesture) const
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return nullptr;
    // Hidden and locked objects can be selected in the editor; locked ones
    // just cannot be moved or resized.
    return HitTester::topmostAt(*screen, gesture.scenePos,
                                {.tolerance = m_context.sceneHitTolerance(gesture),
                                 .includeLocked = true,
                                 .includeHidden = true});
}

bool SelectTool::additive(const PointerGesture& gesture) const
{
    return gesture.modifiers.testFlag(Qt::ShiftModifier) || m_context.selection().multiSelectMode();
}

void SelectTool::tap(const PointerGesture& gesture)
{
    SelectionModel& selection = m_context.selection();
    HmiObject* hit = objectAt(gesture);
    if (!hit)
    {
        if (!additive(gesture))
            selection.clear();
        return;
    }
    if (additive(gesture))
        selection.toggle(hit->id());
    else
        selection.selectOnly(hit->id());
}

void SelectTool::longPress(const PointerGesture& gesture)
{
    if (HmiObject* hit = objectAt(gesture))
        m_context.selection().toggle(hit->id());
}

void SelectTool::dragBegin(const PointerGesture& gesture)
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return;
    SelectionModel& selection = m_context.selection();
    m_startGeometry.clear();

    // A handle of the single selected object wins over everything else.
    if (selection.size() == 1)
    {
        const HmiObject* selected = screen->object(selection.ids().front());
        if (selected && !selected->isLocked())
        {
            const auto handle = SelectionGeometry::handleAt(selected->geometry(),
                                                            selected->rotation(), gesture.scenePos,
                                                            m_context.sceneHandleHitSize(gesture));
            if (handle)
            {
                m_mode = Mode::Resize;
                m_handle = *handle;
                m_resizeId = selected->id();
                m_resizeRotation = selected->rotation();
                m_startGeometry.insert(m_resizeId, selected->geometry());
                return;
            }
        }
    }

    if (HmiObject* hit = objectAt(gesture))
    {
        if (!selection.contains(hit->id()))
        {
            if (additive(gesture))
                selection.add(hit->id());
            else
                selection.selectOnly(hit->id());
        }
        for (const ObjectId& id : selection.ids())
        {
            const HmiObject* object = screen->object(id);
            if (object && !object->isLocked())
                m_startGeometry.insert(id, object->geometry());
        }
        m_mode = m_startGeometry.isEmpty() ? Mode::None : Mode::Move;
        return;
    }

    // Empty space: a finger pans, unless it was held first; mouse and pen
    // draw a rubber band.
    m_mode = gesture.isTouch() && !gesture.afterLongPress ? Mode::Pan : Mode::RubberBand;
}

void SelectTool::dragUpdate(const PointerGesture& gesture)
{
    ICanvasView* canvas = m_context.canvas();
    if (!canvas)
        return;
    switch (m_mode)
    {
    case Mode::Move:
    {
        const QPointF delta = moveDelta(gesture);
        for (auto it = m_startGeometry.cbegin(); it != m_startGeometry.cend(); ++it)
            canvas->setPreviewGeometry(it.key(), it.value().translated(delta));
        break;
    }
    case Mode::Resize:
        canvas->setPreviewGeometry(m_resizeId, resizedGeometry(gesture));
        break;
    case Mode::RubberBand:
        canvas->setRubberBand(rubberBand(gesture));
        break;
    case Mode::Pan:
        canvas->panBy(gesture.viewDelta);
        break;
    case Mode::None:
        break;
    }
}

void SelectTool::dragEnd(const PointerGesture& gesture)
{
    const Mode mode = m_mode;
    switch (mode)
    {
    case Mode::Move:
    {
        const QPointF delta = moveDelta(gesture);
        if (!delta.isNull())
        {
            std::vector<ObjectId> ids;
            for (const ObjectId& id : m_context.selection().ids())
            {
                if (m_startGeometry.contains(id))
                    ids.push_back(id);
            }
            m_context.undoStack().push(new MoveObjectsCommand(
                m_context.project(), m_context.screenId(), std::move(ids), delta));
        }
        break;
    }
    case Mode::Resize:
    {
        const QRectF before = m_startGeometry.value(m_resizeId);
        const QRectF after = resizedGeometry(gesture);
        if (after != before)
        {
            m_context.undoStack().push(new ResizeObjectCommand(
                m_context.project(), m_context.screenId(), m_resizeId, before, after));
        }
        break;
    }
    case Mode::RubberBand:
        if (const Screen* screen = m_context.screen())
        {
            // Dragging right selects enclosed objects; dragging left also
            // selects objects the band only touches.
            const auto rectMode = gesture.scenePos.x() >= gesture.startScenePos.x()
                                      ? HitTester::RectMode::Contains
                                      : HitTester::RectMode::Intersects;
            const auto found = HitTester::inRect(*screen, rubberBand(gesture), rectMode,
                                                 {.includeLocked = true, .includeHidden = true});
            std::vector<ObjectId> ids;
            if (additive(gesture))
                ids = m_context.selection().ids();
            for (const HmiObject* object : found)
                ids.push_back(object->id());
            m_context.selection().set(std::move(ids));
        }
        break;
    case Mode::Pan:
    case Mode::None:
        break;
    }
    cancel();
}

void SelectTool::cancel()
{
    if (ICanvasView* canvas = m_context.canvas())
    {
        canvas->clearPreview();
        canvas->setRubberBand(std::nullopt);
    }
    m_mode = Mode::None;
    m_startGeometry.clear();
}

QPointF SelectTool::moveDelta(const PointerGesture& gesture) const
{
    const QPointF delta = gesture.scenePos - gesture.startScenePos;
    return {std::round(delta.x()), std::round(delta.y())};
}

QRectF SelectTool::resizedGeometry(const PointerGesture& gesture) const
{
    const QRectF start = m_startGeometry.value(m_resizeId);
    return roundedRect(SelectionGeometry::resized(start, m_resizeRotation, m_handle,
                                                  gesture.scenePos - gesture.startScenePos));
}

QRectF SelectTool::rubberBand(const PointerGesture& gesture) const
{
    return QRectF(gesture.startScenePos, gesture.scenePos).normalized();
}

} // namespace hmi
