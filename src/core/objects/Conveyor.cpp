#include "core/objects/Conveyor.h"

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

Conveyor::Conveyor(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Conveyor::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {180.0, 40.0},
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
        });
    return table;
}

std::unique_ptr<HmiObject> Conveyor::clone() const
{
    return std::unique_ptr<HmiObject>(new Conveyor(*this));
}

void Conveyor::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (SymbolObject).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double half = value<double>(Prop::LineWidth) / 2.0;
    const QRectF belt = bounds.adjusted(half, half, -half, -half);
    const double drum = belt.height();
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(value<bool>(Prop::Running) ? Prop::OnColor : Prop::OffColor));
    painter.drawRoundedRect(belt, drum / 2.0, drum / 2.0);
    painter.setBrush(QColor(0xEE, 0xF0, 0xF2));
    const double roller = drum * 0.6;
    const int count = std::max(2, int(belt.width() / (drum * 1.2)));
    for (int i = 0; i < count; ++i)
    {
        const double x = belt.left() + drum / 2.0 + (belt.width() - drum) * i / (count - 1);
        painter.drawEllipse(QPointF(x, belt.center().y()), roller / 2.0, roller / 2.0);
    }
}

QPainterPath Conveyor::hitShape() const
{
    const QPainterPath shape = [this]() -> QPainterPath
    {
        [[maybe_unused]] const QSizeF size = designSize();
        [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
        QPainterPath path;
        path.addRoundedRect(bounds, size.height() / 2.0, size.height() / 2.0);
        return path;
    }();
    return designTransform().map(shape);
}

void Conveyor::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Conveyor, "conveyor", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Conveyor"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/conveyor.svg")
