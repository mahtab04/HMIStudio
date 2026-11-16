#include "core/objects/Bar.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

namespace hmi
{

namespace
{

} // namespace

Bar::Bar(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Bar::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {40.0, 140.0},
        {
            Describe::value(40.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::orientation(Describe::kVertical),
            Describe::styled(Describe::fillColor(QColor(0x3B, 0x82, 0xF6),
                                                 QT_TRANSLATE_NOOP("hmi::Property", "Bar colour")),
                             StyleRole::Accent),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Background"), Appearance,
                            QColor(0xE5, 0xE7, 0xEB)),
            Describe::lineColor(QColor(0x9A, 0xA1, 0xAB)),
            Describe::flag(Prop::ShowScale, "showScale",
                           QT_TRANSLATE_NOOP("hmi::Property", "Scale"), Appearance, true),
        });
    return table;
}

std::unique_ptr<HmiObject> Bar::clone() const
{
    return std::unique_ptr<HmiObject>(new Bar(*this));
}

void Bar::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const bool vertical = value<EnumValue>(Prop::Orientation).value == Describe::kVertical;
    const double part = PaintHelpers::fraction(
        value<double>(Prop::Value), value<double>(Prop::Minimum), value<double>(Prop::Maximum));
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    painter.setBrush(value<QColor>(Prop::BackgroundColor));
    const QRectF frame = bounds.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRect(frame);
    const QRectF filled =
        vertical ? QRectF(frame.left(), frame.bottom() - frame.height() * part, frame.width(),
                          frame.height() * part)
                 : QRectF(frame.left(), frame.top(), frame.width() * part, frame.height());
    painter.fillRect(filled, value<QColor>(Prop::FillColor));
    if (value<bool>(Prop::ShowScale) && !context.lowDetail())
    {
        painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
        constexpr int kTicks = 10;
        for (int i = 1; i < kTicks; ++i)
        {
            const double t = double(i) / kTicks;
            const double tick = i % 5 == 0 ? 0.35 : 0.2;
            if (vertical)
            {
                const double y = frame.bottom() - frame.height() * t;
                painter.drawLine(QPointF(frame.left(), y),
                                 QPointF(frame.left() + frame.width() * tick, y));
            }
            else
            {
                const double x = frame.left() + frame.width() * t;
                painter.drawLine(QPointF(x, frame.bottom()),
                                 QPointF(x, frame.bottom() - frame.height() * tick));
            }
        }
    }
}

void Bar::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Bar, "bar", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Bar"),
                    hmi::ObjectCategory::Elements, ":/icons/bar.svg")
