#include "core/objects/Motor.h"

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

Motor::Motor(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Motor::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {100.0, 64.0},
        {
            Describe::stretchMode(),
            Describe::process(Describe::flag(Prop::Running, "running",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Running"),
                                             Behaviour, false, true)),
            Describe::color(Prop::OnColor, "onColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Running colour"), Appearance,
                            QColor(0x22, 0xA0, 0x55), true),
            Describe::color(Prop::OffColor, "offColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Stopped colour"), Appearance,
                            QColor(0x9A, 0xA1, 0xAB), true),
            Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
            Describe::lineWidth(2.0),
            Describe::textColor(QColor(Qt::white), StyleRole::None),
            Describe::font(18, true),
        });
    return table;
}

std::unique_ptr<HmiObject> Motor::clone() const
{
    return std::unique_ptr<HmiObject>(new Motor(*this));
}

void Motor::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (SymbolObject).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double half = value<double>(Prop::LineWidth) / 2.0;
    const QColor state = value<QColor>(value<bool>(Prop::Running) ? Prop::OnColor : Prop::OffColor);
    const QRectF housing(half, half, size.width() * 0.8 - half, size.height() - 2 * half);
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(state);
    painter.drawRoundedRect(housing, 4.0, 4.0);
    const QRectF shaft(housing.right(), size.height() * 0.4, size.width() - housing.right() - half,
                       size.height() * 0.2);
    painter.setBrush(QColor(0xC9, 0xCE, 0xD6));
    painter.drawRect(shaft);
    if (!context.lowDetail())
    {
        for (int i = 1; i < 4; ++i)
        {
            const double x = housing.left() + housing.width() * i / 8.0;
            painter.drawLine(QPointF(x, housing.top() + 3), QPointF(x, housing.bottom() - 3));
        }
    }
    const double badge = std::min(housing.height(), housing.width() / 2.0) * 0.7;
    const QRectF circle(housing.center().x() + housing.width() * 0.08,
                        housing.center().y() - badge / 2.0, badge, badge);
    painter.setBrush(state.darker(130));
    painter.drawEllipse(circle);
    PaintHelpers::drawText(painter, *this, circle, Qt::AlignCenter, QStringLiteral("M"));
}

void Motor::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Motor, "motor", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Motor"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/motor.svg")
