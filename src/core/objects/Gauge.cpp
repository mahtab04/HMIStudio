#include "core/objects/Gauge.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

} // namespace

Gauge::Gauge(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Gauge::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 140.0},
        {
            Describe::stretchMode(),
            Describe::value(60.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::string(Prop::Unit, "unit", QT_TRANSLATE_NOOP("hmi::Property", "Unit"), Text,
                             QStringLiteral("%")),
            Describe::styled(Describe::fillColor(QColor(Qt::white),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Dial colour")),
                             StyleRole::Face),
            Describe::lineColor(QColor(0x4B, 0x52, 0x5C)),
            Describe::color(Prop::AccentColor, "accentColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Needle colour"), Appearance,
                            QColor(0xDC, 0x26, 0x26)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14, true),
        });
    return table;
}

std::unique_ptr<HmiObject> Gauge::clone() const
{
    return std::unique_ptr<HmiObject>(new Gauge(*this));
}

void Gauge::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (a picture).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double side = std::min(size.width(), size.height());
    const QRectF dial((size.width() - side) / 2.0 + 1.0, (size.height() - side) / 2.0 + 1.0,
                      side - 2.0, side - 2.0);
    const QPointF centre = dial.center();
    const double radius = dial.width() / 2.0;
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.5));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(dial);

    // 0 % at the lower left (225 degrees), 100 % at the lower right (-45).
    const auto angleFor = [](double part)
    {
        return qDegreesToRadians(225.0 - 270.0 * part);
    };
    const auto at = [&](double part, double r)
    {
        return centre + QPointF(std::cos(angleFor(part)) * r, -std::sin(angleFor(part)) * r);
    };
    if (!context.lowDetail())
    {
        constexpr int kTicks = 10;
        for (int i = 0; i <= kTicks; ++i)
        {
            const double t = double(i) / kTicks;
            painter.drawLine(at(t, radius * 0.92), at(t, radius * (i % 5 == 0 ? 0.74 : 0.82)));
        }
    }
    const double part = PaintHelpers::fraction(
        value<double>(Prop::Value), value<double>(Prop::Minimum), value<double>(Prop::Maximum));
    QPen needle(value<QColor>(Prop::AccentColor), std::max(2.0, radius / 20.0));
    needle.setCapStyle(Qt::RoundCap);
    painter.setPen(needle);
    painter.drawLine(centre, at(part, radius * 0.78));
    painter.setPen(Qt::NoPen);
    painter.setBrush(value<QColor>(Prop::LineColor));
    painter.drawEllipse(centre, radius * 0.07, radius * 0.07);
    const QString unit = value<QString>(Prop::Unit);
    const QString text = QString::number(value<double>(Prop::Value), 'f', 0) +
                         (unit.isEmpty() ? QString() : u' ' + unit);
    PaintHelpers::drawText(
        painter, *this, QRectF(dial.left(), centre.y() + radius * 0.25, dial.width(), radius * 0.5),
        Qt::AlignHCenter | Qt::AlignTop, text);
}

void Gauge::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Gauge, "gauge", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Gauge"),
                    hmi::ObjectCategory::Elements, ":/icons/gauge.svg")
