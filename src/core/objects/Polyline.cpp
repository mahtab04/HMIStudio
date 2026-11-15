#include "core/objects/Polyline.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

namespace hmi
{

namespace
{

} // namespace

Polyline::Polyline(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Polyline::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 60.0},
        {
            Describe::lineColor(QColor(0x2B, 0x2F, 0x36)),
            Describe::lineWidth(2.0),
            Describe::points(PointList{{0.0, 1.0}, {0.33, 0.0}, {0.67, 1.0}, {1.0, 0.0}}, 2),
        });
    return table;
}

std::unique_ptr<HmiObject> Polyline::clone() const
{
    return std::unique_ptr<HmiObject>(new Polyline(*this));
}

void Polyline::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(PaintHelpers::toLocal(value<PointList>(Prop::Points), size));
}

QPainterPath Polyline::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    QPainterPath path;
    path.addPolygon(PaintHelpers::toLocal(value<PointList>(Prop::Points), size));
    return PaintHelpers::strokeArea(path, value<double>(Prop::LineWidth));
}

void Polyline::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Polyline, "polyline", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Polyline"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/polyline.svg")
