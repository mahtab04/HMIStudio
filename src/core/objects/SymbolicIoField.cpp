#include "core/objects/SymbolicIoField.h"

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

SymbolicIoField::SymbolicIoField(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> SymbolicIoField::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 34.0},
        {
            Describe::items(QStringLiteral("Stopped;Running;Fault")),
            Describe::integer(Prop::SelectedIndex, "selectedIndex",
                              QT_TRANSLATE_NOOP("hmi::Property", "Value"), Behaviour, 0, 0,
                              std::nullopt, true),
            Describe::styled(Describe::fillColor(QColor(Qt::white),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                             StyleRole::Face),
            Describe::styled(Describe::lineColor(QColor(0x9A, 0xA1, 0xAB)), StyleRole::Line),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14),
        });
    return table;
}

std::unique_ptr<HmiObject> SymbolicIoField::clone() const
{
    return std::unique_ptr<HmiObject>(new SymbolicIoField(*this));
}

void SymbolicIoField::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 3.0);
    const QStringList items = Describe::splitItems(value<QString>(Prop::Items));
    const int index = value<int>(Prop::SelectedIndex);
    const QString shown = index < items.size() ? items[index] : QStringLiteral("#%1").arg(index);
    const double button = std::min(size.height(), size.width() / 3.0);
    const QRectF drop(size.width() - button, 0, button, size.height());
    const double k = PaintHelpers::contentScale(*this);
    PaintHelpers::drawText(painter, *this, bounds.adjusted(6 * k, 0, -button - 4 * k, 0),
                           Qt::AlignLeft | Qt::AlignVCenter, shown, k);
    PaintHelpers::drawRaisedFace(painter, drop.adjusted(2, 2, -2, -2), QColor(0xC9, 0xCE, 0xD6),
                                 QColor(0x7C, 0x85, 0x90), 2.0);
    const double a = button * 0.18;
    const QPointF c = drop.center() + QPointF(0, a * 0.35);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x2B, 0x2F, 0x36));
    painter.drawPolygon(
        QPolygonF{c + QPointF(-a, -a * 0.7), c + QPointF(a, -a * 0.7), c + QPointF(0, a * 0.5)});
}

void SymbolicIoField::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::SymbolicIoField, "symbolicIoField",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Symbolic I/O field"),
                    hmi::ObjectCategory::Elements, ":/icons/symbolic-io-field.svg")
