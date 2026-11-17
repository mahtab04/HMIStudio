#include "core/objects/CheckBox.h"

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

CheckBox::CheckBox(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> CheckBox::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 28.0},
        {
            Describe::process(Describe::flag(Prop::Checked, "checked",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Checked"),
                                             Behaviour, true, true)),
            Describe::text(QStringLiteral("Check box")),
            Describe::styled(Describe::color(Prop::AccentColor, "accentColor",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Mark colour"),
                                             Appearance, QColor(0x3B, 0x82, 0xF6)),
                             StyleRole::Accent),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14),
        });
    return table;
}

std::unique_ptr<HmiObject> CheckBox::clone() const
{
    return std::unique_ptr<HmiObject>(new CheckBox(*this));
}

void CheckBox::paint(QPainter& painter, const PaintContext& context) const
{
    // Control and label grow with the height; extra width is for the label.
    [[maybe_unused]] const QSizeF size = PaintHelpers::scaleToHeight(painter, *this);
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double side = size.height() * 0.72;
    const QRectF box(1.0, (size.height() - side) / 2.0, side, side);
    const bool checked = value<bool>(Prop::Checked);
    painter.setPen(
        QPen(checked ? value<QColor>(Prop::AccentColor) : QColor(0x9A, 0xA1, 0xAB), 1.5));
    painter.setBrush(checked ? value<QColor>(Prop::AccentColor) : QColor(Qt::white));
    painter.drawRoundedRect(box, 3.0, 3.0);
    if (checked)
    {
        QPen tick(Qt::white, std::max(1.5, side / 8.0));
        tick.setCapStyle(Qt::RoundCap);
        tick.setJoinStyle(Qt::RoundJoin);
        painter.setPen(tick);
        const QPolygonF mark{box.topLeft() + QPointF(side * 0.22, side * 0.52),
                             box.topLeft() + QPointF(side * 0.42, side * 0.72),
                             box.topLeft() + QPointF(side * 0.78, side * 0.3)};
        painter.drawPolyline(mark);
    }
    PaintHelpers::drawText(painter, *this, bounds.adjusted(side + 8.0, 0, 0, 0),
                           Qt::AlignLeft | Qt::AlignVCenter, value<QString>(Prop::Text));
}

RuntimeBehavior CheckBox::runtimeBehavior() const
{
    return {RuntimeBehavior::Kind::Toggle, Prop::Checked};
}

void CheckBox::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::CheckBox, "checkBox",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Check box"),
                    hmi::ObjectCategory::Elements, ":/icons/check-box.svg")
