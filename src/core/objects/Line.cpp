#include "core/objects/Line.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>
#include <array>

namespace hmi
{

namespace
{

constexpr std::array kDirectionOptions{
    EnumOption{.value = Line::Falling,
               .key = "falling",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Top-left to bottom-right")},
    EnumOption{.value = Line::Rising,
               .key = "rising",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Bottom-left to top-right")},
};

// Thinnest stroke used for hit testing, so a hairline can still be picked.
constexpr double kMinimumHitWidth = 1.0;

} // namespace

Line::Line(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Line::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {100.0, 0.0}, {
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
                           .defaultValue = 2.0,
                           .constraint = {.minimum = 0.0, .maximum = 50.0}},
                          {.id = Prop::LineDirection,
                           .key = "lineDirection",
                           .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Direction"),
                           .category = Layout,
                           .defaultValue = EnumValue{Falling},
                           .constraint = {.enumOptions = kDirectionOptions}},
                      });
    return table;
}

std::unique_ptr<HmiObject> Line::clone() const
{
    return std::unique_ptr<HmiObject>(new Line(*this));
}

QLineF Line::localLine() const
{
    const QSizeF size = geometry().size();
    if (value<EnumValue>(Prop::LineDirection).value == Rising)
        return {0.0, size.height(), size.width(), 0.0};
    return {0.0, 0.0, size.width(), size.height()};
}

void Line::paint(QPainter& painter, const PaintContext& /*context*/) const
{
    const double lineWidth = value<double>(Prop::LineWidth);
    if (lineWidth <= 0.0)
        return;
    QPen pen(value<QColor>(Prop::LineColor), lineWidth);
    pen.setCapStyle(Qt::FlatCap);
    painter.setPen(pen);
    painter.drawLine(localLine());
}

QPainterPath Line::hitShape() const
{
    QPainterPath centre;
    const QLineF line = localLine();
    centre.moveTo(line.p1());
    centre.lineTo(line.p2());

    QPainterPathStroker stroker;
    stroker.setWidth(std::max(value<double>(Prop::LineWidth), kMinimumHitWidth));
    stroker.setCapStyle(Qt::FlatCap);
    return stroker.createStroke(centre);
}

void Line::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Line, "line", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Line"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/line.svg")
