#include "quick/overlays/SelectionGeometry.h"

#include "core/model/HmiObject.h"

#include <QTransform>

#include <algorithm>
#include <cmath>

namespace hmi::SelectionGeometry
{

namespace
{

std::array<QPointF, kHandleCount> localHandles(QSizeF size)
{
    const double w = size.width();
    const double h = size.height();
    return {QPointF(0, 0), QPointF(w / 2, 0), QPointF(w, 0), QPointF(w, h / 2),
            QPointF(w, h), QPointF(w / 2, h), QPointF(0, h), QPointF(0, h / 2)};
}

bool movesLeft(SelectionHandle h)
{
    return h == SelectionHandle::TopLeft || h == SelectionHandle::Left ||
           h == SelectionHandle::BottomLeft;
}
bool movesRight(SelectionHandle h)
{
    return h == SelectionHandle::TopRight || h == SelectionHandle::Right ||
           h == SelectionHandle::BottomRight;
}
bool movesTop(SelectionHandle h)
{
    return h == SelectionHandle::TopLeft || h == SelectionHandle::Top ||
           h == SelectionHandle::TopRight;
}
bool movesBottom(SelectionHandle h)
{
    return h == SelectionHandle::BottomLeft || h == SelectionHandle::Bottom ||
           h == SelectionHandle::BottomRight;
}

} // namespace

QPolygonF frame(const QRectF& geometry, double rotation)
{
    return HmiObject::sceneTransform(geometry, rotation)
        .map(QPolygonF(QRectF(QPointF(0, 0), geometry.size())));
}

std::array<QPointF, kHandleCount> handles(const QRectF& geometry, double rotation)
{
    const QTransform t = HmiObject::sceneTransform(geometry, rotation);
    auto points = localHandles(geometry.size());
    for (QPointF& p : points)
        p = t.map(p);
    return points;
}

std::optional<SelectionHandle> handleAt(const QRectF& geometry, double rotation, QPointF scenePos,
                                        double hitSize)
{
    const auto points = handles(geometry, rotation);
    const double half = hitSize / 2.0;
    std::optional<SelectionHandle> best;
    double bestDistance = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const QPointF d = scenePos - points[i];
        if (std::abs(d.x()) > half || std::abs(d.y()) > half)
            continue;
        const double distance = std::hypot(d.x(), d.y());
        if (!best || distance < bestDistance)
        {
            best = static_cast<SelectionHandle>(i);
            bestDistance = distance;
        }
    }
    return best;
}

QRectF resized(const QRectF& geometry, double rotation, SelectionHandle handle, QPointF sceneDelta,
               double minimumSize)
{
    // Work in the object's unrotated local frame.
    const QPointF local = QTransform().rotate(-rotation).map(sceneDelta);
    const double minW = geometry.width() > 0.0 ? minimumSize : 0.0;
    const double minH = geometry.height() > 0.0 ? minimumSize : 0.0;

    double left = 0.0;
    double top = 0.0;
    double right = geometry.width();
    double bottom = geometry.height();
    if (movesLeft(handle))
        left = std::min(left + local.x(), right - minW);
    if (movesRight(handle))
        right = std::max(right + local.x(), left + minW);
    if (movesTop(handle))
        top = std::min(top + local.y(), bottom - minH);
    if (movesBottom(handle))
        bottom = std::max(bottom + local.y(), top + minH);

    // The new rectangle, expressed in the old local frame, keeps its place in
    // the scene if its centre maps to the same scene point.
    const QRectF localRect(QPointF(left, top), QPointF(right, bottom));
    const QPointF centre = HmiObject::sceneTransform(geometry, rotation).map(localRect.center());
    const QSizeF size = localRect.size();
    return {centre - QPointF(size.width() / 2.0, size.height() / 2.0), size};
}

} // namespace hmi::SelectionGeometry
