#include "core/objects/Sensor.h"

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

constexpr EnumOption kKindOptions[] = {
    {.value = Sensor::Temperature,
     .key = "temperature",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Temperature")},
    {.value = Sensor::Pressure,
     .key = "pressure",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Pressure")},
    {.value = Sensor::Level,
     .key = "level",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Level")},
    {.value = Sensor::Flow,
     .key = "flow",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Flow")},
};

// P&ID letters for the kinds, in kind order.
constexpr const char* kLetters[] = {"T", "P", "L", "F"};

} // namespace

Sensor::Sensor(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Sensor::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {70.0, 90.0}, {
                          Describe::stretchMode(),
                          Describe::choice(Prop::SensorKind, "sensorKind",
                                           QT_TRANSLATE_NOOP("hmi::Property", "Measures"),
                                           Behaviour, Temperature, kKindOptions),
                          Describe::text(QStringLiteral("TT-101")),
                          Describe::fillColor(QColor(Qt::white)),
                          Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
                          Describe::lineWidth(2.0),
                          Describe::textColor(QColor(0x1F, 0x23, 0x28)),
                          Describe::font(13, true),
                      });
    return table;
}

std::unique_ptr<HmiObject> Sensor::clone() const
{
    return std::unique_ptr<HmiObject>(new Sensor(*this));
}

void Sensor::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (SymbolObject).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double side = std::min(size.width(), size.height() * 0.72);
    const double half = value<double>(Prop::LineWidth) / 2.0;
    const QRectF bubble =
        QRectF((size.width() - side) / 2.0, 0, side, side).adjusted(half, half, -half, -half);
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(Prop::FillColor));
    painter.drawEllipse(bubble);
    painter.drawLine(QPointF(bubble.left(), bubble.center().y()),
                     QPointF(bubble.right(), bubble.center().y()));
    const int kind = std::clamp(value<EnumValue>(Prop::SensorKind).value, 0, 3);
    PaintHelpers::drawText(
        painter, *this, QRectF(bubble.left(), bubble.top(), bubble.width(), bubble.height() / 2.0),
        Qt::AlignHCenter | Qt::AlignBottom, QString::fromLatin1(kLetters[kind]));
    PaintHelpers::drawText(painter, *this, QRectF(0, side, size.width(), size.height() - side),
                           Qt::AlignCenter, value<QString>(Prop::Text));
}

void Sensor::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Sensor, "sensor", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Sensor"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/sensor.svg")
