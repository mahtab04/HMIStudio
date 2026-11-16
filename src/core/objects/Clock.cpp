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
        {120.0, 120.0},
        {
            Describe::stretchMode(),
            Describe::choice(Prop::ClockMode, "clockMode",
                             QT_TRANSLATE_NOOP("hmi::Property", "Style"), Appearance, Analog,
                             kModeOptions),
            Describe::styled(Describe::fillColor(QColor(Qt::white),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Face colour")),
                             StyleRole::Face),
            Describe::lineColor(QColor(0x2B, 0x2F, 0x36)),
            Describe::color(Prop::AccentColor, "accentColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Second hand colour"), Appearance,
                            QColor(0xDC, 0x26, 0x26)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(20, true),
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
    if (value<EnumValue>(Prop::ClockMode).value == Digital)
    {
        PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                               value<QColor>(Prop::LineColor), 4.0);
        PaintHelpers::drawText(painter, *this, bounds, Qt::AlignCenter,
                               QStringLiteral("%1:%2:%3")
                                   .arg(kHour, 2, 10, QChar(u'0'))
                                   .arg(kMinute, 2, 10, QChar(u'0'))
                                   .arg(kSecond, 2, 10, QChar(u'0')));
        return;
    }
    const double side = std::min(size.width(), size.height());
    const QRectF face((size.width() - side) / 2.0 + 1.0, (size.height() - side) / 2.0 + 1.0,
                      side - 2.0, side - 2.0);
    const QPointF centre = face.center();
    const double radius = face.width() / 2.0;
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 2.0));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(face);
    // Clockwise from 12 o'clock.
    const auto at = [&](double turns, double r)
    {
        const double a = turns * 2.0 * std::numbers::pi;
        return centre + QPointF(std::sin(a) * r, -std::cos(a) * r);
    };
    for (int i = 0; i < 12; ++i)
        painter.drawLine(at(i / 12.0, radius * 0.9), at(i / 12.0, radius * 0.78));
    const auto hand = [&](double turns, double length, double width, const QColor& color)
    {
        QPen pen(color, width);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(centre, at(turns, radius * length));
    };
    const QColor line = value<QColor>(Prop::LineColor);
    hand((kHour + kMinute / 60.0) / 12.0, 0.5, std::max(2.5, radius / 14.0), line);
    hand(kMinute / 60.0, 0.75, std::max(2.0, radius / 20.0), line);
    hand(kSecond / 60.0, 0.82, 1.0, value<QColor>(Prop::AccentColor));
}

void Clock::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Clock, "clock", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Clock"),
                    hmi::ObjectCategory::Elements, ":/icons/clock.svg")
