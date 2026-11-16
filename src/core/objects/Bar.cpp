#include "core/objects/Bar.h"

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

Bar::Bar(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Bar::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {64.0, 170.0},
        {
            Describe::value(40.0),
            Describe::minimum(0.0),
            Describe::maximum(100.0),
            Describe::orientation(Describe::kVertical),
            Describe::fillColor(QColor(0x7D, 0xBB, 0x2B),
                                QT_TRANSLATE_NOOP("hmi::Property", "Bar colour")),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Body colour"), Appearance,
                            QColor(0x4A, 0x4F, 0x57)),
            Describe::lineColor(QColor(0xD9, 0xDC, 0xE1)),
            Describe::flag(Prop::ShowScale, "showScale",
                           QT_TRANSLATE_NOOP("hmi::Property", "Scale"), Appearance, true),
            Describe::textColor(QColor(0xD9, 0xDC, 0xE1), StyleRole::None),
            Describe::font(10),
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
    // Body.
    const QColor body = value<QColor>(Prop::BackgroundColor);
    QLinearGradient shade(bounds.topLeft(), vertical ? bounds.topRight() : bounds.bottomLeft());
    shade.setColorAt(0.0, body.lighter(115));
    shade.setColorAt(1.0, body.darker(115));
    painter.setPen(QPen(body.darker(140), 1.0));
    painter.setBrush(shade);
    painter.drawRoundedRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);

    const double pad = std::min(size.width(), size.height()) * 0.08 + 2.0;
    const double across = vertical ? size.width() : size.height();
    const double trackThickness = std::max(6.0, across * 0.3);
    QRectF track;
    if (vertical)
        track = QRectF(size.width() - pad - trackThickness, pad, trackThickness,
                       size.height() - 2 * pad);
    else
        track = QRectF(pad, size.height() - pad - trackThickness, size.width() - 2 * pad,
                       trackThickness);
    painter.fillRect(track, body.darker(150));
    const QRectF filled =
        vertical ? QRectF(track.left(), track.bottom() - track.height() * part, track.width(),
                          track.height() * part)
                 : QRectF(track.left(), track.top(), track.width() * part, track.height());
    QLinearGradient level(filled.topLeft(), vertical ? filled.topRight() : filled.bottomLeft());
    const QColor fill = value<QColor>(Prop::FillColor);
    level.setColorAt(0.0, fill.lighter(125));
    level.setColorAt(1.0, fill.darker(110));
    painter.fillRect(filled, level);

    if (value<bool>(Prop::ShowScale))
    {
        const double tick = across * 0.12;
        const QLineF axis =
            vertical ? QLineF(track.left() - 3, track.bottom(), track.left() - 3, track.top())
                     : QLineF(track.left(), track.top() - 3, track.right(), track.top() - 3);
        PaintHelpers::drawScale(painter, axis, vertical ? QPointF(-1, 0) : QPointF(0, -1), tick,
                                value<double>(Prop::Minimum), value<double>(Prop::Maximum),
                                value<QColor>(Prop::LineColor),
                                PaintHelpers::scaledFont(*this, PaintHelpers::contentScale(*this)),
                                !context.lowDetail());
    }
}

void Bar::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Bar, "bar", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Bar"),
                    hmi::ObjectCategory::Elements, ":/icons/bar.svg")
