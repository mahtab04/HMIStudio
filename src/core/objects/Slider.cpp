#include "core/objects/Slider.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

} // namespace

Slider::Slider(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Slider::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {200.0, 60.0},
        {
            Describe::value(50.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::orientation(Describe::kHorizontal),
            Describe::color(Prop::AccentColor, "accentColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Track colour"), Appearance,
                            QColor(0x7D, 0xBB, 0x2B)),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Background"), Appearance,
                            QColor(0x4A, 0x4F, 0x57)),
            Describe::lineColor(QColor(0x6B, 0x72, 0x80)),
            Describe::flag(Prop::ShowScale, "showScale",
                           QT_TRANSLATE_NOOP("hmi::Property", "Scale"), Appearance, true),
            Describe::textColor(QColor(0x4B, 0x52, 0x5C)),
            Describe::font(10),
        });
    return table;
}

std::unique_ptr<HmiObject> Slider::clone() const
{
    return std::unique_ptr<HmiObject>(new Slider(*this));
}

void Slider::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const bool vertical = value<EnumValue>(Prop::Orientation).value == Describe::kVertical;
    const double part = PaintHelpers::fraction(
        value<double>(Prop::Value), value<double>(Prop::Minimum), value<double>(Prop::Maximum));
    const double across = vertical ? size.width() : size.height();
    const double along = vertical ? size.height() : size.width();
    const double knob = across * 0.42;
    const double thickness = std::max(2.0, across * 0.11);
    const double length = std::max(0.0, along - knob);
    // Track near the top (or left), scale under (or beside) it.
    const double line = knob / 2.0 + 2.0;
    const auto at = [&](double t)
    {
        return vertical ? QPointF(line, knob / 2.0 + length * (1.0 - t))
                        : QPointF(knob / 2.0 + length * t, line);
    };
    const QPointF start = at(0.0);
    const QPointF end = at(1.0);
    const QPointF current = at(part);
    const auto bar = [&](QPointF from, QPointF to)
    {
        return vertical ? QRectF(QPointF(from.x() - thickness / 2.0, std::min(from.y(), to.y())),
                                 QSizeF(thickness, std::abs(to.y() - from.y())))
                        : QRectF(QPointF(std::min(from.x(), to.x()), from.y() - thickness / 2.0),
                                 QSizeF(std::abs(to.x() - from.x()), thickness));
    };
    painter.setPen(Qt::NoPen);
    painter.setBrush(value<QColor>(Prop::BackgroundColor));
    painter.drawRoundedRect(bar(start, end), thickness / 2.0, thickness / 2.0);
    painter.setBrush(value<QColor>(Prop::AccentColor));
    painter.drawRoundedRect(bar(start, current), thickness / 2.0, thickness / 2.0);

    if (value<bool>(Prop::ShowScale))
    {
        const double offset = line + knob / 2.0 + 2.0;
        const QLineF axis = vertical ? QLineF(offset, start.y(), offset, end.y())
                                     : QLineF(start.x(), offset, end.x(), offset);
        PaintHelpers::drawScale(painter, axis, vertical ? QPointF(1, 0) : QPointF(0, 1),
                                across * 0.1, value<double>(Prop::Minimum),
                                value<double>(Prop::Maximum), value<QColor>(Prop::LineColor),
                                PaintHelpers::scaledFont(*this, PaintHelpers::contentScale(*this)),
                                !context.lowDetail());
    }

    QRadialGradient shine(current - QPointF(knob * 0.15, knob * 0.18), knob * 0.65);
    shine.setColorAt(0.0, Qt::white);
    shine.setColorAt(1.0, QColor(0xB8, 0xBE, 0xC7));
    painter.setPen(QPen(QColor(0x7C, 0x85, 0x90), 1.0));
    painter.setBrush(shine);
    painter.drawEllipse(current, knob / 2.0 - 0.5, knob / 2.0 - 0.5);
}

void Slider::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Slider, "slider", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Slider"),
                    hmi::ObjectCategory::Elements, ":/icons/slider.svg")
