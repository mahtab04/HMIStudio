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
        {140.0, 36.0},
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
            Describe::font(16),
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
    const double arrow = std::min(size.height() * 0.6, 20.0);
    PaintHelpers::drawText(painter, *this, bounds.adjusted(6, 0, -arrow - 6, 0),
                           Qt::AlignLeft | Qt::AlignVCenter, shown);
    const QPointF tip(size.width() - arrow / 2.0 - 4.0, size.height() / 2.0 + arrow / 6.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(value<QColor>(Prop::TextColor));
    painter.drawPolygon(QPolygonF{tip + QPointF(-arrow / 4.0, -arrow / 4.0),
                                  tip + QPointF(arrow / 4.0, -arrow / 4.0), tip});
}

void SymbolicIoField::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::SymbolicIoField, "symbolicIoField",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Symbolic I/O field"),
                    hmi::ObjectCategory::Elements, ":/icons/symbolic-io-field.svg")
