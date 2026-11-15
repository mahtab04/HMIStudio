#include "core/objects/RadioGroup.h"

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

RadioGroup::RadioGroup(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> RadioGroup::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 90.0}, {
                           Describe::items(QStringLiteral("Option 1;Option 2;Option 3")),
                           Describe::integer(Prop::SelectedIndex, "selectedIndex",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Selected"),
                                             Behaviour, 0, -1, std::nullopt, true),
                           Describe::color(Prop::AccentColor, "accentColor",
                                           QT_TRANSLATE_NOOP("hmi::Property", "Mark colour"),
                                           Appearance, QColor(0x3B, 0x82, 0xF6)),
                           Describe::textColor(QColor(0x1F, 0x23, 0x28)),
                           Describe::font(14),
                       });
    return table;
}

std::unique_ptr<HmiObject> RadioGroup::clone() const
{
    return std::unique_ptr<HmiObject>(new RadioGroup(*this));
}

void RadioGroup::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const QStringList items = Describe::splitItems(value<QString>(Prop::Items));
    if (items.isEmpty())
        return;
    const double rowHeight = size.height() / items.size();
    const double side = std::min(rowHeight * 0.7, 18.0);
    for (int i = 0; i < items.size(); ++i)
    {
        const QRectF row(0, i * rowHeight, size.width(), rowHeight);
        const QRectF dot(1.0, row.center().y() - side / 2.0, side, side);
        painter.setPen(QPen(QColor(0x9A, 0xA1, 0xAB), 1.5));
        painter.setBrush(Qt::white);
        painter.drawEllipse(dot);
        if (i == value<int>(Prop::SelectedIndex))
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(value<QColor>(Prop::AccentColor));
            painter.drawEllipse(dot.adjusted(side * 0.25, side * 0.25, -side * 0.25, -side * 0.25));
        }
        PaintHelpers::drawText(painter, *this, row.adjusted(side + 8.0, 0, 0, 0),
                               Qt::AlignLeft | Qt::AlignVCenter, items[i]);
    }
}

void RadioGroup::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::RadioGroup, "radioGroup",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Radio group"),
                    hmi::ObjectCategory::Elements, ":/icons/radio-group.svg")
