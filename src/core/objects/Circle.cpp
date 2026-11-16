#include "core/objects/Circle.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>

namespace hmi
{

namespace
{

} // namespace

Circle::Circle(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Circle::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table =
        withCommonDescriptors({80.0, 80.0}, {
                                                Describe::fillColor(QColor(0xC6, 0xD0, 0xDE)),
                                                Describe::lineColor(QColor(0x4F, 0x5F, 0x78)),
                                                Describe::lineWidth(1.0),
                                            });
    return table;
}

std::unique_ptr<HmiObject> Circle::clone() const
{
    return std::unique_ptr<HmiObject>(new Circle(*this));
}

void Circle::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double half = value<double>(Prop::LineWidth) / 2.0;
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(circleRect().adjusted(half, half, -half, -half));
}

QPainterPath Circle::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    QPainterPath path;
    path.addEllipse(circleRect());
    return path;
}

QRectF Circle::circleRect() const
{
    const QSizeF size = geometry().size();
    const double side = std::min(size.width(), size.height());
    return {(size.width() - side) / 2.0, (size.height() - side) / 2.0, side, side};
}

void Circle::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Circle, "circle", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Circle"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/circle.svg")
