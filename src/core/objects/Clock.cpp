#include "core/objects/Clock.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace hmi
{

namespace
{

constexpr EnumOption kModeOptions[] = {
    {.value = Clock::Analog,
     .key = "analog",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Analogue")},
    {.value = Clock::Digital,
     .key = "digital",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Digital")},
};

// The time shown in the editor.
constexpr int kHour = 10;
constexpr int kMinute = 10;
constexpr int kSecond = 30;

} // namespace

Clock::Clock(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Clock::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {150.0, 150.0}, {
                            Describe::stretchMode(),
                            Describe::choice(Prop::ClockMode, "clockMode",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Style"),
                                             Appearance, Analog, kModeOptions),
                            Describe::fillColor(QColor(0x3A, 0x3F, 0x47),
                                                QT_TRANSLATE_NOOP("hmi::Property", "Face colour")),
                            Describe::color(Prop::BackgroundColor, "backgroundColor",
                                            QT_TRANSLATE_NOOP("hmi::Property", "Rim colour"),
                                            Appearance, QColor(0xD9, 0xDC, 0xE1)),
                            Describe::lineColor(QColor(0xC9, 0xCE, 0xD6)),
                            Describe::color(Prop::AccentColor, "accentColor",
                                            QT_TRANSLATE_NOOP("hmi::Property", "Hand colour"),
                                            Appearance, QColor(0x6E, 0xC6, 0xFF)),
                            Describe::textColor(QColor(0x9A, 0xA1, 0xAB), StyleRole::None),
                            Describe::font(14, true),
                        });
    return table;
}

std::unique_ptr<HmiObject> Clock::clone() const
{
    return std::unique_ptr<HmiObject>(new Clock(*this));
}

void Clock::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (a picture).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const QColor faceColor = value<QColor>(Prop::FillColor);
    const QColor accent = value<QColor>(Prop::AccentColor);
    if (value<EnumValue>(Prop::ClockMode).value == Digital)
    {
        const QRectF panel(0, size.height() * 0.3, size.width(), size.height() * 0.4);
        PaintHelpers::drawFace(painter, panel, faceColor, value<QColor>(Prop::BackgroundColor),
                               6.0);
        QFont digits = value<QFont>(Prop::Font);
        digits.setPixelSize(std::max(1, int(panel.height() * 0.5)));
        painter.setFont(digits);
        painter.setPen(accent);
        painter.drawText(panel, Qt::AlignCenter,
                         QStringLiteral("%1:%2:%3")
                             .arg(kHour, 2, 10, QChar(u'0'))
                             .arg(kMinute, 2, 10, QChar(u'0'))
                             .arg(kSecond, 2, 10, QChar(u'0')));
        return;
    }
    const double side = std::min(size.width(), size.height());
    const QPointF centre(size.width() / 2.0, size.height() / 2.0);
    const double radius = side / 2.0 - 1.0;

    // Rim, then the dark face.
    const QColor rim = value<QColor>(Prop::BackgroundColor);
    QLinearGradient rimShade(centre - QPointF(0, radius), centre + QPointF(0, radius));
    rimShade.setColorAt(0.0, rim.lighter(115));
    rimShade.setColorAt(1.0, rim.darker(135));
    painter.setPen(QPen(rim.darker(150), 1.0));
    painter.setBrush(rimShade);
    painter.drawEllipse(centre, radius, radius);
    const double faceRadius = radius * 0.9;
    QRadialGradient face(centre - QPointF(0, faceRadius * 0.4), faceRadius * 1.3);
    face.setColorAt(0.0, faceColor.lighter(135));
    face.setColorAt(1.0, faceColor.darker(115));
    painter.setPen(Qt::NoPen);
    painter.setBrush(face);
    painter.drawEllipse(centre, faceRadius, faceRadius);

    // Clockwise from 12 o'clock.
    const auto at = [&](double turns, double r)
    {
        const double a = turns * 2.0 * std::numbers::pi;
        return centre + QPointF(std::sin(a) * r, -std::cos(a) * r);
    };
    const QColor tick = value<QColor>(Prop::LineColor);
    for (int i = 0; i < 60; ++i)
    {
        const bool major = i % 5 == 0;
        if (!major && context.lowDetail())
            continue;
        painter.setPen(QPen(tick, major ? radius * 0.03 : radius * 0.012));
        painter.drawLine(at(i / 60.0, faceRadius * 0.95),
                         at(i / 60.0, faceRadius * (major ? 0.82 : 0.89)));
    }
    if (!context.lowDetail())
    {
        QFont numbers = value<QFont>(Prop::Font);
        numbers.setPixelSize(std::max(1, int(radius * 0.17)));
        painter.setFont(numbers);
        painter.setPen(value<QColor>(Prop::TextColor));
        for (int h = 1; h <= 12; ++h)
        {
            const QPointF p = at(h / 12.0, faceRadius * 0.66);
            painter.drawText(QRectF(p - QPointF(radius * 0.15, radius * 0.12),
                                    QSizeF(radius * 0.3, radius * 0.24)),
                             Qt::AlignCenter, QString::number(h));
        }
    }
    const auto hand = [&](double turns, double length, double tail, double width)
    {
        QPen pen(accent, width);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(at(turns + 0.5, radius * tail), at(turns, radius * length));
    };
    hand((kHour + kMinute / 60.0) / 12.0, 0.48, 0.0, radius * 0.075);
    hand(kMinute / 60.0, 0.74, 0.0, radius * 0.05);
    hand(kSecond / 60.0, 0.8, 0.16, std::max(1.0, radius * 0.015));
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(centre, radius * 0.07, radius * 0.07);
}

void Clock::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Clock, "clock", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Clock"),
                    hmi::ObjectCategory::Elements, ":/icons/clock.svg")
