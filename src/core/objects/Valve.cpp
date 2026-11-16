#include "core/objects/Valve.h"

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

Valve::Valve(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Valve::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {80.0, 60.0},
        {
            Describe::stretchMode(),
            Describe::flag(Prop::Open, "open", QT_TRANSLATE_NOOP("hmi::Property", "Open"),
                           Behaviour, true, true),
            Describe::color(Prop::OnColor, "onColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Open colour"), Appearance,
                            QColor(0x22, 0xA0, 0x55), true),
            Describe::color(Prop::OffColor, "offColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Closed colour"), Appearance,
                            QColor(0xDC, 0x26, 0x26), true),
            Describe::lineColor(QColor(0x3A, 0x40, 0x48)),
            Describe::lineWidth(2.0),
        });
    return table;
}

std::unique_ptr<HmiObject> Valve::clone() const
{
    return std::unique_ptr<HmiObject>(new Valve(*this));
}

void Valve::paint(QPainter& painter, const PaintContext& context) const
{
    // Drawn at the design size and scaled to the rectangle (SymbolObject).
    painter.setTransform(designTransform(), true);
    [[maybe_unused]] const QSizeF size = designSize();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const double half = value<double>(Prop::LineWidth) / 2.0;
    painter.setPen(PaintHelpers::linePen(*this));
    painter.setBrush(value<QColor>(value<bool>(Prop::Open) ? Prop::OnColor : Prop::OffColor));
    painter.save();
    // Keep the outline inside the bounds.
    painter.translate(half, half);
    painter.scale((size.width() - 2 * half) / std::max(size.width(), 1.0),
                  (size.height() - 2 * half) / std::max(size.height(), 1.0));
    painter.drawPath(bodyPath());
    painter.restore();
    const double top = size.height() * 0.35;
    const double mid = (top + size.height()) / 2.0;
    painter.drawLine(QPointF(size.width() / 2.0, mid), QPointF(size.width() / 2.0, half + 4.0));
    painter.drawRect(QRectF(size.width() * 0.3, half, size.width() * 0.4, size.height() * 0.12));
}

QPainterPath Valve::hitShape() const
{
    const QPainterPath shape = [this]() -> QPainterPath
    {
        [[maybe_unused]] const QSizeF size = designSize();
        [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
        QPainterPath path = bodyPath();
        path.addRect(QRectF(size.width() * 0.3, 0, size.width() * 0.4, size.height() * 0.35));
        return path;
    }();
    return designTransform().map(shape);
}

QPainterPath Valve::bodyPath() const
{
    const QSizeF size = designSize();
    const double top = size.height() * 0.35;
    const double bottom = size.height();
    const double mid = (top + bottom) / 2.0;
    QPainterPath path;
    path.addPolygon(QPolygonF{{0, top}, {size.width() / 2.0, mid}, {0, bottom}});
    path.closeSubpath();
    path.addPolygon(
        QPolygonF{{size.width(), top}, {size.width() / 2.0, mid}, {size.width(), bottom}});
    path.closeSubpath();
    return path;
}

void Valve::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Valve, "valve", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Valve"),
                    hmi::ObjectCategory::ProcessSymbols, ":/icons/valve.svg")
