#include "core/objects/Pump.h"

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

Pump::Pump(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Pump::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {80.0, 80.0},
        {
            Describe::flag(Prop::Running, "running", QT_TRANSLATE_NOOP("hmi::Property", "Running"),
                           Behaviour, false, true),
            Describe::color(Prop::OnColor, "onColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Running colour"), Appearance,
                            QColor(0x22, 0xA0, 0x55), true),
            Describe::color(Prop::OffColor, "offColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Stopped colour"), Appearance,
                            QColor(0x9A, 0xA1, 0xAB), true),
            Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
            Describe::lineWidth(2.0),
        });
    return table;
}

std::unique_ptr<HmiObject> Pump::clone() const
{
    return std::unique_ptr<HmiObject>(new Pump(*this));
}

void Pump::paint(QPainter& painter, const PaintContext& context) const
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
    painter.setBrush(value<QColor>(value<bool>(Prop::Running) ? Prop::OnColor : Prop::OffColor));
    painter.drawEllipse(body);
    const QPointF c = body.center();
    const double r = body.width() / 2.0;
    painter.setBrush(Qt::white);
    painter.drawPolygon(QPolygonF{c + QPointF(-r * 0.45, -r * 0.6), c + QPointF(r * 0.75, 0),
                                  c + QPointF(-r * 0.45, r * 0.6)});
}

QPainterPath Pump::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    const double side = std::min(size.width(), size.height());
    QPainterPath path;
    path.addEllipse(QRectF((size.width() - side) / 2.0, (size.height() - side) / 2.0, side, side));
    return path;
}

void Pump::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Pump, "pump", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Pump"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/pump.svg")
