#include "core/objects/Polygon.h"

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

Polygon::Polygon(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Polygon::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {100.0, 100.0},
        {
            Describe::fillColor(QColor(0xC6, 0xD0, 0xDE)),
            Describe::lineColor(QColor(0x4F, 0x5F, 0x78)),
            Describe::lineWidth(1.0),
            Describe::points(
                PointList{{0.5, 0.0}, {1.0, 0.38}, {0.81, 1.0}, {0.19, 1.0}, {0.0, 0.38}}, 3),
        });
    return table;
}

std::unique_ptr<HmiObject> Polygon::clone() const
{
    return std::unique_ptr<HmiObject>(new Polygon(*this));
}

void Polygon::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawPolygon(PaintHelpers::toLocal(value<PointList>(Prop::Points), size));
}

QPainterPath Polygon::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    QPainterPath path;
    path.addPolygon(PaintHelpers::toLocal(value<PointList>(Prop::Points), size));
    path.closeSubpath();
    return path.united(PaintHelpers::strokeArea(path, value<double>(Prop::LineWidth)));
}

void Polygon::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_PATH_OBJECT(hmi::Polygon, "polygon",
                         QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Polygon"),
                         hmi::ObjectCategory::BasicObjects, ":/icons/polygon.svg", Points)
