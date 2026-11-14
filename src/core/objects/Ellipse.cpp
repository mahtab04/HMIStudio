#include "core/objects/Ellipse.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

namespace hmi
{

Ellipse::Ellipse(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Ellipse::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {80.0, 80.0}, {
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
                      });
    return table;
}

std::unique_ptr<HmiObject> Ellipse::clone() const
{
    return std::unique_ptr<HmiObject>(new Ellipse(*this));
}

void Ellipse::paint(QPainter& painter, const PaintContext& /*context*/) const
{
    const double lineWidth = value<double>(Prop::LineWidth);
    const double half = lineWidth / 2.0;
    const QRectF outline =
        QRectF(QPointF(0, 0), geometry().size()).adjusted(half, half, -half, -half);

    painter.setPen(lineWidth > 0.0 ? QPen(value<QColor>(Prop::LineColor), lineWidth)
                                   : QPen(Qt::NoPen));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(outline);
}

QPainterPath Ellipse::hitShape() const
{
    QPainterPath path;
    path.addEllipse(QRectF(QPointF(0, 0), geometry().size()));
    return path;
}

void Ellipse::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Ellipse, "ellipse", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Ellipse"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/ellipse.svg")
