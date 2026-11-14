#include "core/objects/Rectangle.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>

namespace hmi
{

Rectangle::Rectangle(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Rectangle::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {100.0, 60.0}, {
                           {.id = Prop::FillColor,
                            .key = "fillColor",
                            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Fill colour"),
                            .category = Appearance,
                            .defaultValue = QColor(0xEE, 0xF0, 0xF2),
                            .bindable = true},
                           {.id = Prop::LineColor,
                            .key = "lineColor",
                            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Line colour"),
                            .category = Appearance,
                            .defaultValue = QColor(0x2B, 0x2F, 0x36),
                            .bindable = true},
                           {.id = Prop::LineWidth,
                            .key = "lineWidth",
                            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Line width"),
                            .category = Appearance,
                            .defaultValue = 1.0,
                            .constraint = {.minimum = 0.0, .maximum = 50.0}},
                           {.id = Prop::CornerRadius,
                            .key = "cornerRadius",
                            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Corner radius"),
                            .category = Appearance,
                            .defaultValue = 0.0,
                            .constraint = {.minimum = 0.0, .maximum = 1000.0}},
                       });
    return table;
}

std::unique_ptr<HmiObject> Rectangle::clone() const
{
    return std::unique_ptr<HmiObject>(new Rectangle(*this));
}

void Rectangle::paint(QPainter& painter, const PaintContext& /*context*/) const
{
    const double lineWidth = value<double>(Prop::LineWidth);
    const double half = lineWidth / 2.0;
    const QRectF outline =
        QRectF(QPointF(0, 0), geometry().size()).adjusted(half, half, -half, -half);
    const double radius = std::min(value<double>(Prop::CornerRadius),
                                   std::min(outline.width(), outline.height()) / 2.0);

    painter.setPen(lineWidth > 0.0 ? QPen(value<QColor>(Prop::LineColor), lineWidth)
                                   : QPen(Qt::NoPen));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawRoundedRect(outline, radius, radius);
}

QPainterPath Rectangle::hitShape() const
{
    const QRectF bounds(QPointF(0, 0), geometry().size());
    const double radius = std::min(value<double>(Prop::CornerRadius),
                                   std::min(bounds.width(), bounds.height()) / 2.0);
    QPainterPath path;
    path.addRoundedRect(bounds, radius, radius);
    return path;
}

void Rectangle::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Rectangle, "rectangle",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Rectangle"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/rectangle.svg")
