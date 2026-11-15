#include "core/objects/Switch.h"

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

Switch::Switch(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Switch::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 36.0},
        {
            Describe::flag(Prop::Checked, "checked", QT_TRANSLATE_NOOP("hmi::Property", "On"),
                           Behaviour, false, true),
            Describe::string(Prop::OnText, "onText", QT_TRANSLATE_NOOP("hmi::Property", "On text"),
                             Text, QStringLiteral("On")),
            Describe::string(Prop::OffText, "offText",
                             QT_TRANSLATE_NOOP("hmi::Property", "Off text"), Text,
                             QStringLiteral("Off")),
            Describe::color(Prop::OnColor, "onColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "On colour"), Appearance,
                            QColor(0x22, 0xA0, 0x55)),
            Describe::color(Prop::OffColor, "offColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Off colour"), Appearance,
                            QColor(0xB8, 0xBE, 0xC6)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14),
        });
    return table;
}

std::unique_ptr<HmiObject> Switch::clone() const
{
    return std::unique_ptr<HmiObject>(new Switch(*this));
}

void Switch::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const bool on = value<bool>(Prop::Checked);
    const double trackHeight = std::min(size.height(), 28.0);
    const double trackWidth = std::min(size.width(), trackHeight * 1.9);
    const QRectF track(0, (size.height() - trackHeight) / 2.0, trackWidth, trackHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(value<QColor>(on ? Prop::OnColor : Prop::OffColor));
    painter.drawRoundedRect(track, trackHeight / 2.0, trackHeight / 2.0);
    const double knob = trackHeight - 6.0;
    const double knobX = on ? track.right() - knob - 3.0 : track.left() + 3.0;
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(knobX, track.top() + 3.0, knob, knob));
    PaintHelpers::drawText(painter, *this, bounds.adjusted(trackWidth + 8.0, 0, 0, 0),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           value<QString>(on ? Prop::OnText : Prop::OffText));
}

void Switch::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Switch, "switch", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Switch"),
                    hmi::ObjectCategory::Elements, ":/icons/switch.svg")
