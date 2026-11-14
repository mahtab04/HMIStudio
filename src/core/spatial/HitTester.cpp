#include "core/spatial/HitTester.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"

#include <QPainterPathStroker>
#include <QPolygonF>

#include <ranges>

namespace hmi
{

namespace
{

bool isCandidate(const HmiObject& object, const HitOptions& options)
{
    return (options.includeHidden || object.isVisible()) &&
           (options.includeLocked || !object.isLocked());
}

} // namespace

bool HitTester::hits(const HmiObject& object, QPointF scenePos, double tolerance)
{
    bool invertible = false;
    const QTransform toLocal = object.sceneTransform().inverted(&invertible);
    if (!invertible)
        return false;

    const QPointF local = toLocal.map(scenePos);
    const QPainterPath shape = object.hitShape();
    if (shape.contains(local))
        return true;
    if (tolerance <= 0.0)
        return false;

    QPainterPathStroker stroker;
    stroker.setWidth(2.0 * tolerance);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(shape).contains(local);
}

HmiObject* HitTester::topmostAt(const Screen& screen, QPointF scenePos, const HitOptions& options)
{
    for (const auto& object : std::views::reverse(screen.objects()))
    {
        if (isCandidate(*object, options) && hits(*object, scenePos, options.tolerance))
            return object.get();
    }
    return nullptr;
}

std::vector<HmiObject*> HitTester::allAt(const Screen& screen, QPointF scenePos,
                                         const HitOptions& options)
{
    std::vector<HmiObject*> result;
    for (const auto& object : std::views::reverse(screen.objects()))
    {
        if (isCandidate(*object, options) && hits(*object, scenePos, options.tolerance))
            result.push_back(object.get());
    }
    return result;
}

std::vector<HmiObject*> HitTester::inRect(const Screen& screen, const QRectF& sceneRect,
                                          RectMode mode, const HitOptions& options)
{
    const QRectF band = sceneRect.normalized();
    std::vector<HmiObject*> result;
    for (const auto& object : screen.objects())
    {
        if (!isCandidate(*object, options))
            continue;

        bool selected = false;
        if (mode == RectMode::Contains)
        {
            selected = band.contains(object->sceneBoundingRect());
        }
        else
        {
            bool invertible = false;
            const QTransform toLocal = object->sceneTransform().inverted(&invertible);
            if (invertible)
            {
                QPainterPath localBand;
                localBand.addPolygon(toLocal.map(QPolygonF(band)));
                localBand.closeSubpath();
                selected = object->hitShape().intersects(localBand);
            }
        }
        if (selected)
            result.push_back(object.get());
    }
    return result;
}

} // namespace hmi
