#include "editor/tools/PathTool.h"

#include "core/model/HmiObject.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/tools/ObjectPlacement.h"
#include "editor/tools/ToolContext.h"
#include "quick/ICanvasView.h"

#include <algorithm>
#include <cmath>

namespace hmi
{

PathTool::PathTool(ToolContext& context)
    : m_context(context)
{
}

void PathTool::setType(ObjectTypeId type)
{
    cancel();
    m_type = type;
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(type);
    m_orthogonal = info && info->placement == Placement::OrthogonalPoints;
}

void PathTool::deactivate()
{
    cancel();
}

int PathTool::minimumPoints() const
{
    // The type says how many points it needs, through its descriptor.
    const auto object = ObjectRegistry::instance().create(m_type);
    const PropertyDescriptor* d =
        object ? findDescriptor(object->descriptors(), Prop::Points) : nullptr;
    return d && d->constraint.minimum ? std::max(2, int(*d->constraint.minimum)) : 2;
}

QPointF PathTool::constrained(QPointF scenePos, const PointerGesture& gesture) const
{
    QPointF p = scenePos;
    const double grid = m_context.snapSettings().gridSpacing;
    if (m_context.snapEnabled() && m_context.snapSettings().toGrid && grid > 0.0)
        p = QPointF(std::round(p.x() / grid) * grid, std::round(p.y() / grid) * grid);
    else
        p = QPointF(std::round(p.x()), std::round(p.y()));

    if (m_orthogonal && !m_points.empty() && !gesture.modifiers.testFlag(Qt::AltModifier))
    {
        // Keep the longer direction; the segment is horizontal or vertical.
        const QPointF last = m_points.back();
        const QPointF d = p - last;
        p = std::abs(d.x()) >= std::abs(d.y()) ? QPointF(p.x(), last.y())
                                               : QPointF(last.x(), p.y());
    }
    return p;
}

void PathTool::addPoint(QPointF point)
{
    if (!m_points.empty() && m_points.back() == point)
        return;
    m_points.push_back(point);
    showPreview();
}

void PathTool::showPreview(const std::vector<QPointF>& extra)
{
    ICanvasView* canvas = m_context.canvas();
    if (!canvas)
        return;
    std::vector<QPointF> path = m_points;
    path.insert(path.end(), extra.begin(), extra.end());
    canvas->setPathPreview(path);
}

void PathTool::handleGesture(const PointerGesture& gesture)
{
    using Kind = PointerGesture::Kind;
    switch (gesture.kind)
    {
    case Kind::Tap:
        addPoint(constrained(gesture.scenePos, gesture));
        break;
    case Kind::DoubleTap:
        // The first tap of the pair already added the point.
        finish();
        break;
    case Kind::DragBegin:
        if (m_points.empty())
            addPoint(constrained(gesture.startScenePos, gesture));
        break;
    case Kind::DragUpdate:
        showPreview({constrained(gesture.scenePos, gesture)});
        break;
    case Kind::DragEnd:
        addPoint(constrained(gesture.scenePos, gesture));
        break;
    case Kind::DragCancel:
        showPreview();
        break;
    default:
        break;
    }
}

bool PathTool::keyPress(int key, Qt::KeyboardModifiers /*modifiers*/)
{
    switch (key)
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finish();
        return true;
    case Qt::Key_Backspace:
        if (m_points.empty())
            return false;
        m_points.pop_back();
        showPreview();
        return true;
    case Qt::Key_Escape:
        if (m_points.empty())
            m_context.requestSelectTool();
        else
            cancel();
        return true;
    default:
        return false;
    }
}

void PathTool::cancel()
{
    m_points.clear();
    if (ICanvasView* canvas = m_context.canvas())
        canvas->setPathPreview({});
}

void PathTool::finish()
{
    if (int(m_points.size()) < minimumPoints())
    {
        cancel();
        return;
    }
    // Bounds around the points; points are stored relative to them. A
    // straight horizontal or vertical run has zero height or width.
    double left = m_points.front().x();
    double top = m_points.front().y();
    double right = left;
    double bottom = top;
    for (const QPointF& p : m_points)
    {
        left = std::min(left, p.x());
        top = std::min(top, p.y());
        right = std::max(right, p.x());
        bottom = std::max(bottom, p.y());
    }
    const QRectF bounds(QPointF(left, top), QPointF(right, bottom));
    PointList relative;
    for (const QPointF& p : m_points)
    {
        relative.append(QPointF(bounds.width() > 0 ? (p.x() - left) / bounds.width() : 0.0,
                                bounds.height() > 0 ? (p.y() - top) / bounds.height() : 0.0));
    }

    auto object = ObjectRegistry::instance().create(m_type);
    cancel();
    if (!object || !ResizeObjectCommand::setGeometry(*object, bounds) ||
        !object->setProperty(Prop::Points, relative))
        return;
    if (ObjectPlacement::add(m_context, std::move(object)))
        m_context.requestSelectTool();
}

} // namespace hmi
