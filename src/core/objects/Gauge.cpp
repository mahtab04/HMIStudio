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
        {160.0, 160.0},
        {
            Describe::stretchMode(),
            Describe::value(60.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::string(Prop::Unit, "unit", QT_TRANSLATE_NOOP("hmi::Property", "Unit"), Text,
                             QString()),
            Describe::fillColor(QColor(0xE9, 0xEB, 0xEE),
                                QT_TRANSLATE_NOOP("hmi::Property", "Dial colour")),
            Describe::lineColor(QColor(0x5F, 0x64, 0x6C)),
            Describe::color(Prop::AccentColor, "accentColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Value colour"), Appearance,
                            QColor(0x7D, 0xBB, 0x2B)),
            Describe::color(Prop::OnColor, "onColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Needle colour"), Appearance,
                            QColor(0xE5, 0x53, 0x3D)),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Centre colour"), Appearance,
                            QColor(0x2B, 0x2F, 0x36)),
            Describe::textColor(QColor(0x4B, 0x52, 0x5C)),
            Describe::font(12, true),
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
    const QPointF centre(size.width() / 2.0, size.height() / 2.0);
    const double radius = side / 2.0 - 1.0;

    // Dial face, lit from above.
    const QColor face = value<QColor>(Prop::FillColor);
    QRadialGradient dial(centre - QPointF(0, radius * 0.35), radius * 1.3);
    dial.setColorAt(0.0, face.lighter(106));
    dial.setColorAt(1.0, face.darker(112));
    painter.setPen(QPen(face.darker(130), 1.0));
    painter.setBrush(dial);
    painter.drawEllipse(centre, radius, radius);

    // 0 at the lower left (225 degrees), the maximum at the lower right.
    const auto angleFor = [](double part)
    {
        return qDegreesToRadians(225.0 - 270.0 * part);
    };
    const auto at = [&](double part, double r)
    {
        return centre + QPointF(std::cos(angleFor(part)) * r, -std::sin(angleFor(part)) * r);
    };
    const double minimum = value<double>(Prop::Minimum);
    const double maximum = value<double>(Prop::Maximum);
    const double part = PaintHelpers::fraction(value<double>(Prop::Value), minimum, maximum);

    const double bandRadius = radius * 0.72;
    const double bandWidth = radius * 0.11;
    const QRectF band(centre - QPointF(bandRadius, bandRadius),
                      QSizeF(2 * bandRadius, 2 * bandRadius));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(value<QColor>(Prop::LineColor), bandWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(band, 225 * 16, -270 * 16);
    painter.setPen(
        QPen(value<QColor>(Prop::AccentColor), bandWidth * 0.6, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(band, 225 * 16, int(std::lround(-270.0 * 16.0 * part)));

    // Ticks outside the band; numbers inside it.
    const QColor ink = value<QColor>(Prop::TextColor);
    painter.setPen(QPen(ink, 1.2));
    constexpr int kDivisions = 5;
    constexpr int kMinor = 5;
    for (int i = 0; i <= kDivisions * kMinor; ++i)
    {
        const double t = double(i) / (kDivisions * kMinor);
        const bool major = i % kMinor == 0;
        painter.drawLine(at(t, radius * 0.82), at(t, radius * (major ? 0.94 : 0.88)));
    }
    if (!context.lowDetail())
    {
        QFont font = value<QFont>(Prop::Font);
        font.setPixelSize(std::max(1, int(radius * 0.13)));
        painter.setFont(font);
        for (int i = 0; i <= kDivisions; ++i)
        {
            const double t = double(i) / kDivisions;
            const QPointF p = at(t, radius * 0.5);
            const QRectF box(p - QPointF(radius * 0.2, radius * 0.1),
                             QSizeF(radius * 0.4, radius * 0.2));
            painter.drawText(box, Qt::AlignCenter,
                             QString::number(minimum + (maximum - minimum) * t, 'g', 4));
        }
    }

    // Needle.
    QPen needle(value<QColor>(Prop::OnColor), std::max(1.5, radius * 0.035));
    needle.setCapStyle(Qt::RoundCap);
    painter.setPen(needle);
    painter.drawLine(centre, at(part, radius * 0.78));

    // Dark centre with the value.
    const double cap = radius * 0.3;
    const QColor dark = value<QColor>(Prop::BackgroundColor);
    QRadialGradient centreShade(centre - QPointF(cap * 0.3, cap * 0.4), cap * 1.4);
    centreShade.setColorAt(0.0, dark.lighter(170));
    centreShade.setColorAt(1.0, dark);
    painter.setPen(QPen(QColor(0x9A, 0xA1, 0xAB), std::max(1.0, radius * 0.025)));
    painter.setBrush(centreShade);
    painter.drawEllipse(centre, cap, cap);
    QFont digits = value<QFont>(Prop::Font);
    digits.setPixelSize(std::max(1, int(cap * 0.75)));
    digits.setBold(true);
    painter.setFont(digits);
    painter.setPen(value<QColor>(Prop::AccentColor));
    painter.drawText(QRectF(centre - QPointF(cap, cap), QSizeF(2 * cap, 2 * cap)), Qt::AlignCenter,
                     QString::number(value<double>(Prop::Value), 'f', 0));
    const QString unit = value<QString>(Prop::Unit);
    if (!unit.isEmpty() && !context.lowDetail())
    {
        QFont small = digits;
        small.setPixelSize(std::max(1, int(radius * 0.12)));
        small.setBold(false);
        painter.setFont(small);
        painter.setPen(ink);
        painter.drawText(QRectF(centre.x() - radius, centre.y() + cap, 2 * radius, radius * 0.25),
                         Qt::AlignCenter, unit);
    }
}

void Gauge::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Gauge, "gauge", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Gauge"),
                    hmi::ObjectCategory::Elements, ":/icons/gauge.svg")
