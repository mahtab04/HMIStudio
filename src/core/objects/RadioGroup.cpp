#include "core/objects/RadioGroup.h"

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

RadioGroup::RadioGroup(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> RadioGroup::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 110.0},
        {
            Describe::items(QStringLiteral("Option 1;Option 2;Option 3")),
            Describe::integer(Prop::SelectedIndex, "selectedIndex",
                              QT_TRANSLATE_NOOP("hmi::Property", "Selected"), Behaviour, 0, -1,
                              std::nullopt, true),
            Describe::styled(Describe::fillColor(QColor(0xF4, 0xF7, 0xFF),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                             StyleRole::Face),
            Describe::styled(Describe::lineColor(QColor(0x9A, 0xA1, 0xAB)), StyleRole::Line),
            Describe::styled(Describe::color(Prop::AccentColor, "accentColor",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Mark colour"),
                                             Appearance, QColor(0x3B, 0x82, 0xF6)),
                             StyleRole::Accent),
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
    // Control and label grow with the height; extra width is for the label.
    [[maybe_unused]] const QSizeF size = PaintHelpers::scaleToHeight(painter, *this);
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 4.0);
    const QStringList items = Describe::splitItems(value<QString>(Prop::Items));
    if (items.isEmpty())
        return;
    const QRectF inner = bounds.adjusted(8, 4, -6, -4);
    const double rowHeight = inner.height() / items.size();
    const double side = rowHeight * 0.55;
    const QColor accent = value<QColor>(Prop::AccentColor);
    for (int i = 0; i < items.size(); ++i)
    {
        const QRectF row(inner.left(), inner.top() + i * rowHeight, inner.width(), rowHeight);
        const QRectF dot(row.left(), row.center().y() - side / 2.0, side, side);
        // A shaded sphere, lit from the upper left.
        QRadialGradient sphere(dot.center() - QPointF(side * 0.2, side * 0.25), side * 0.8);
        sphere.setColorAt(0.0, QColor(0xFB, 0xFC, 0xFD));
        sphere.setColorAt(1.0, QColor(0xA7, 0xAE, 0xB8));
        painter.setPen(QPen(QColor(0x7C, 0x85, 0x90), 1.0));
        painter.setBrush(sphere);
        painter.drawEllipse(dot);
        if (i == value<int>(Prop::SelectedIndex))
        {
            const QRectF mark = dot.adjusted(side * 0.27, side * 0.27, -side * 0.27, -side * 0.27);
            QRadialGradient fill(mark.center() - QPointF(side * 0.08, side * 0.1), side * 0.35);
            fill.setColorAt(0.0, accent.lighter(150));
            fill.setColorAt(1.0, accent.darker(115));
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawEllipse(mark);
        }
        PaintHelpers::drawText(painter, *this, row.adjusted(side + 10.0, 0, 0, 0),
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
