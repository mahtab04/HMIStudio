#include "core/objects/Tank.h"

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

Tank::Tank(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Tank::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {110.0, 160.0},
        {
            Describe::value(60.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::fillColor(QColor(0x3B, 0x82, 0xF6),
                                QT_TRANSLATE_NOOP("hmi::Property", "Liquid colour")),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Shell colour"), Appearance,
                            QColor(0xEE, 0xF0, 0xF2)),
            Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
            Describe::lineWidth(2.0),
            Describe::flag(Prop::ShowScale, "showScale",
                           QT_TRANSLATE_NOOP("hmi::Property", "Scale"), Appearance, true),
        });
    return table;
}

std::unique_ptr<HmiObject> Tank::clone() const
{
    return std::unique_ptr<HmiObject>(new Tank(*this));
}

void Tank::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const QPainterPath vessel = vesselPath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(value<QColor>(Prop::BackgroundColor));
    painter.drawPath(vessel);

    const double level = PaintHelpers::fraction(
        value<double>(Prop::Value), value<double>(Prop::Minimum), value<double>(Prop::Maximum));
    if (level > 0.0)
    {
        const QRectF box = vessel.boundingRect();
        painter.save();
        painter.setClipPath(vessel);
        painter.fillRect(QRectF(box.left(), box.bottom() - box.height() * level, box.width(),
                                box.height() * level),
                         value<QColor>(Prop::FillColor));
        painter.restore();
    }
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(vessel);

    if (value<bool>(Prop::ShowScale) && !context.lowDetail())
    {
        const QRectF box = vessel.boundingRect();
        painter.save();
        painter.setClipPath(vessel); // the scale stays inside the rounded ends
        painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
        constexpr int kTicks = 10;
        for (int i = 1; i < kTicks; ++i)
        {
            const double y = box.bottom() - box.height() * i / kTicks;
            painter.drawLine(QPointF(box.right() - box.width() * (i % 5 == 0 ? 0.22 : 0.12), y),
                             QPointF(box.right() - 2.0, y));
        }
        painter.restore();
    }
}

QPainterPath Tank::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    return vesselPath();
}

QPainterPath Tank::vesselPath() const
{
    const QSizeF size = geometry().size();
    const double half = value<double>(Prop::LineWidth) / 2.0;
    const QRectF r = QRectF(QPointF(0, 0), size).adjusted(half, half, -half, -half);
    const double cap = std::min(r.width() / 2.0, r.height() / 4.0);
    QPainterPath path;
    path.addRoundedRect(r, r.width() / 2.0, cap);
    return path;
}

void Tank::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Tank, "tank", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Tank"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/tank.svg")
