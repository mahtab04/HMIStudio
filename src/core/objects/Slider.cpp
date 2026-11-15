#include "core/objects/Slider.h"

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

Slider::Slider(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Slider::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {160.0, 36.0},
        {
            Describe::value(50.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::orientation(Describe::kHorizontal),
            Describe::styled(Describe::color(Prop::AccentColor, "accentColor",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Track colour"),
                                             Appearance, QColor(0x3B, 0x82, 0xF6)),
                             StyleRole::Accent),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Background"), Appearance,
                            QColor(0xD1, 0xD5, 0xDB)),
        });
    return table;
}

std::unique_ptr<HmiObject> Slider::clone() const
{
    return std::unique_ptr<HmiObject>(new Slider(*this));
}

void Slider::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const bool vertical = value<EnumValue>(Prop::Orientation).value == Describe::kVertical;
    const double part = PaintHelpers::fraction(
        value<double>(Prop::Value), value<double>(Prop::Minimum), value<double>(Prop::Maximum));
    const double knob = std::min(vertical ? size.width() : size.height(), 24.0);
    const double thickness = std::max(2.0, knob / 4.0);
    const double length = (vertical ? size.height() : size.width()) - knob;
    painter.setPen(Qt::NoPen);
    QPointF centre;
    if (vertical)
    {
        const double x = size.width() / 2.0;
        centre = QPointF(x, knob / 2.0 + length * (1.0 - part));
        painter.setBrush(value<QColor>(Prop::BackgroundColor));
        painter.drawRoundedRect(QRectF(x - thickness / 2.0, knob / 2.0, thickness, length),
                                thickness / 2.0, thickness / 2.0);
        painter.setBrush(value<QColor>(Prop::AccentColor));
        painter.drawRoundedRect(
            QRectF(x - thickness / 2.0, centre.y(), thickness, knob / 2.0 + length - centre.y()),
            thickness / 2.0, thickness / 2.0);
    }
    else
    {
        const double y = size.height() / 2.0;
        centre = QPointF(knob / 2.0 + length * part, y);
        painter.setBrush(value<QColor>(Prop::BackgroundColor));
        painter.drawRoundedRect(QRectF(knob / 2.0, y - thickness / 2.0, length, thickness),
                                thickness / 2.0, thickness / 2.0);
        painter.setBrush(value<QColor>(Prop::AccentColor));
        painter.drawRoundedRect(
            QRectF(knob / 2.0, y - thickness / 2.0, centre.x() - knob / 2.0, thickness),
            thickness / 2.0, thickness / 2.0);
    }
    painter.setPen(QPen(QColor(0x9A, 0xA1, 0xAB), 1.0));
    painter.setBrush(Qt::white);
    painter.drawEllipse(centre, knob / 2.0 - 1.0, knob / 2.0 - 1.0);
}

void Slider::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Slider, "slider", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Slider"),
                    hmi::ObjectCategory::Elements, ":/icons/slider.svg")
