#include "editor/tools/SelectTool.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "core/property/Prop.h"
#include "core/spatial/HitTester.h"
#include "editor/commands/MoveObjectsCommand.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/commands/SetPropertyCommand.h"
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

bool isAxisAligned(double rotation)
{
    return qFuzzyIsNull(std::remainder(rotation, 360.0));
}

Snapper::Edges movingEdges(SelectionHandle h)
{
    using H = SelectionHandle;
    return {.left = h == H::TopLeft || h == H::Left || h == H::BottomLeft,
            .top = h == H::TopLeft || h == H::Top || h == H::TopRight,
            .right = h == H::TopRight || h == H::Right || h == H::BottomRight,
            .bottom = h == H::BottomLeft || h == H::Bottom || h == H::BottomRight};
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
        // The menu acts on the selection, so the object under the pointer
        // joins it first; on empty space the selection is kept.
        if (HmiObject* hit = objectAt(gesture); hit && !m_context.selection().contains(hit->id()))
            m_context.selection().selectOnly(hit->id());
        m_context.requestContextMenu(gesture.viewPos);
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
            const QRectF geometry = selected->geometry();
            const double rotation = selected->rotation();
            const double hitSize = m_context.sceneHandleHitSize(gesture);
            const auto handle =
                SelectionGeometry::handleAt(geometry, rotation, gesture.scenePos, hitSize);

            // The rotation handle sits close to the top handle; with touch
            // their hit areas overlap, and the nearer one wins.
            const double offset = m_context.input().rotationHandleOffset / m_context.zoom();
            const QPointF rotationHandle =
                SelectionGeometry::rotationHandle(geometry, rotation, offset);
            const auto distance = [&](QPointF p)
            {
                return QLineF(gesture.scenePos, p).length();
            };
            if (SelectionGeometry::hitsRotationHandle(geometry, rotation, offset, gesture.scenePos,
                                                      hitSize) &&
                (!handle ||
                 distance(rotationHandle) < distance(SelectionGeometry::handles(
                                                geometry, rotation)[std::size_t(*handle)])))
            {
                m_mode = Mode::Rotate;
                m_resizeId = selected->id();
                m_resizeRotation = rotation;
                m_startGeometry.insert(m_resizeId, geometry);
                return;
            }
            if (handle)
            {
                m_mode = Mode::Resize;
                m_handle = *handle;
                m_resizeId = selected->id();
                m_resizeRotation = selected->rotation();
                m_startGeometry.insert(m_resizeId, selected->geometry());
                startSnapper();
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
        m_moveBounds = QRectF();
        for (const ObjectId& id : selection.ids())
        {
            const HmiObject* object = screen->object(id);
            if (object && !object->isLocked())
            {
                m_startGeometry.insert(id, object->geometry());
                const QRectF bounds = object->sceneBoundingRect();
                m_moveBounds = m_moveBounds.isNull() ? bounds : m_moveBounds.united(bounds);
            }
        }
        m_mode = m_startGeometry.isEmpty() ? Mode::None : Mode::Move;
        if (m_mode == Mode::Move)
            startSnapper();
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
        canvas->setGuides(m_guides);
        break;
    }
    case Mode::Resize:
        canvas->setPreviewGeometry(m_resizeId, resizedGeometry(gesture));
        canvas->setGuides(m_guides);
        break;
    case Mode::Rotate:
        canvas->setPreviewRotation(m_resizeId, rotationFor(gesture));
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
    case Mode::Rotate:
    {
        const double rotation = rotationFor(gesture);
        if (!qFuzzyCompare(rotation, m_resizeRotation))
        {
            m_context.undoStack().push(new SetPropertyCommand(
                m_context.project(), m_context.screenId(), {m_resizeId}, Prop::Rotation, rotation));
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
        canvas->setGuides({});
    }
    m_mode = Mode::None;
    m_startGeometry.clear();
    m_snapper.reset();
    m_guides.clear();
}

bool SelectTool::snapping(const PointerGesture& gesture) const
{
    return m_snapper && !gesture.modifiers.testFlag(Qt::AltModifier);
}

void SelectTool::startSnapper()
{
    m_snapper.reset();
    const Screen* screen = m_context.screen();
    if (!screen || !m_context.snapEnabled())
        return;
    std::vector<ObjectId> moving(m_startGeometry.keyBegin(), m_startGeometry.keyEnd());
    m_snapper.emplace(*screen, moving, m_context.snapSettings());
}

QPointF SelectTool::moveDelta(const PointerGesture& gesture) const
{
    const QPointF raw = gesture.scenePos - gesture.startScenePos;
    QPointF delta(std::round(raw.x()), std::round(raw.y()));
    m_guides.clear();
    if (snapping(gesture))
    {
        const SnapResult snap = m_snapper->snapMove(m_moveBounds.translated(delta),
                                                    m_context.sceneSnapDistance(gesture));
        delta += snap.offset;
        m_guides = snap.guides;
    }
    return delta;
}

QRectF SelectTool::resizedGeometry(const PointerGesture& gesture) const
{
    const QRectF start = m_startGeometry.value(m_resizeId);
    QRectF result = roundedRect(SelectionGeometry::resized(
        start, m_resizeRotation, m_handle, gesture.scenePos - gesture.startScenePos));
    m_guides.clear();
    // Edges of a rotated object do not line up with anything axis-aligned.
    if (snapping(gesture) && isAxisAligned(m_resizeRotation))
    {
        const Snapper::Edges edges = movingEdges(m_handle);
        const SnapResult snap =
            m_snapper->snapResize(result, edges, m_context.sceneSnapDistance(gesture));
        QRectF snapped = result;
        if (edges.left)
            snapped.setLeft(snapped.left() + snap.offset.x());
        if (edges.right)
            snapped.setRight(snapped.right() + snap.offset.x());
        if (edges.top)
            snapped.setTop(snapped.top() + snap.offset.y());
        if (edges.bottom)
            snapped.setBottom(snapped.bottom() + snap.offset.y());
        // Never snap an object inside out or to nothing.
        if (snapped.width() >= 1.0 && snapped.height() >= 1.0)
        {
            result = snapped;
            m_guides = snap.guides;
        }
    }
    return result;
}

double SelectTool::rotationFor(const PointerGesture& gesture) const
{
    const QRectF geometry = m_startGeometry.value(m_resizeId);
    const double raw = SelectionGeometry::rotationTowards(geometry, gesture.scenePos);
    double rotation = std::round(raw);
    if (gesture.modifiers.testFlag(Qt::ShiftModifier))
    {
        rotation = std::round(raw / kRotationStep) * kRotationStep;
    }
    else if (m_context.snapEnabled())
    {
        const double nearest = std::round(raw / 45.0) * 45.0;
        if (std::abs(raw - nearest) <= kRotationSnap)
            rotation = nearest;
    }
    // Keep the value in (-180, 180] so the same angle is stored the same way.
    if (rotation <= -180.0)
        rotation += 360.0;
    return rotation == 0.0 ? 0.0 : rotation; // no negative zero
}

QRectF SelectTool::rubberBand(const PointerGesture& gesture) const
{
    return QRectF(gesture.startScenePos, gesture.scenePos).normalized();
}

} // namespace hmi
