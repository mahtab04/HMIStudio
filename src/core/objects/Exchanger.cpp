#include "core/objects/Exchanger.h"

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

Exchanger::Exchanger(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Exchanger::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {90.0, 90.0}, {
                          Describe::fillColor(QColor(0xC9, 0xCE, 0xD6)),
                          Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
                          Describe::lineWidth(2.0),
                          Describe::color(Prop::AccentColor, "accentColor",
                                          QT_TRANSLATE_NOOP("hmi::Property", "Tube colour"),
                                          Appearance, QColor(0xDC, 0x26, 0x26), true),
                      });
    return table;
}

std::unique_ptr<HmiObject> Exchanger::clone() const
{
    return std::unique_ptr<HmiObject>(new Exchanger(*this));
}

void Exchanger::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double side = std::min(size.width(), size.height());
    const double half = value<double>(Prop::LineWidth) / 2.0;
    const QRectF body =
        QRectF((size.width() - side) / 2.0, (size.height() - side) / 2.0, side, side)
            .adjusted(half, half, -half, -half);
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(body);
    const double r = body.width() / 2.0;
    const QPointF c = body.center();
    QPen tube(value<QColor>(Prop::AccentColor), std::max(2.0, r / 12.0));
    tube.setJoinStyle(Qt::RoundJoin);
    painter.setPen(tube);
    painter.drawPolyline(QPolygonF{c + QPointF(-r, 0), c + QPointF(-r * 0.5, 0),
                                   c + QPointF(-r * 0.25, -r * 0.45), c + QPointF(0, r * 0.45),
                                   c + QPointF(r * 0.25, -r * 0.45), c + QPointF(r * 0.5, 0),
                                   c + QPointF(r, 0)});
}

QPainterPath Exchanger::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    const double side = std::min(size.width(), size.height());
    QPainterPath path;
    path.addEllipse(QRectF((size.width() - side) / 2.0, (size.height() - side) / 2.0, side, side));
    return path;
}

void Exchanger::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Exchanger, "exchanger",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Heat exchanger"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/exchanger.svg")
