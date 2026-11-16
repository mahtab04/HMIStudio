#include "core/objects/Pipe.h"

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

Pipe::Pipe(ObjectId id)
    : SymbolObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Pipe::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {160.0, 80.0},
        {
            Describe::points(PointList{{0.0, 0.0}, {0.5, 0.0}, {0.5, 1.0}, {1.0, 1.0}}, 2),
            Describe::lineColor(QColor(0x7A, 0x82, 0x8C)),
            Describe::lineWidth(10.0),
            Describe::process(Describe::flag(Prop::Flowing, "flowing",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Flowing"),
                                             Behaviour, false, true)),
            Describe::color(Prop::FlowColor, "flowColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Flow colour"), Appearance,
                            QColor(0x3B, 0x82, 0xF6), true),
        });
    return table;
}

std::unique_ptr<HmiObject> Pipe::clone() const
{
    return std::unique_ptr<HmiObject>(new Pipe(*this));
}

void Pipe::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const QPolygonF route = PaintHelpers::toLocal(value<PointList>(Prop::Points), size);
    const double width = value<double>(Prop::LineWidth);
    if (width <= 0.0 || route.size() < 2)
        return;
    QPen outer(value<QColor>(Prop::LineColor).darker(125), width);
    outer.setJoinStyle(Qt::MiterJoin);
    outer.setCapStyle(Qt::FlatCap);
    painter.setPen(outer);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(route);
    QPen inner(value<QColor>(Prop::LineColor), std::max(1.0, width - 3.0));
    inner.setJoinStyle(Qt::MiterJoin);
    inner.setCapStyle(Qt::FlatCap);
    painter.setPen(inner);
    painter.drawPolyline(route);
    if (value<bool>(Prop::Flowing))
    {
        QPen flow(value<QColor>(Prop::FlowColor), std::max(1.0, width / 3.0), Qt::DashLine);
        flow.setCapStyle(Qt::FlatCap);
        painter.setPen(flow);
        painter.drawPolyline(route);
    }
}

QPainterPath Pipe::hitShape() const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    QPainterPath path;
    path.addPolygon(PaintHelpers::toLocal(value<PointList>(Prop::Points), size));
    return PaintHelpers::strokeArea(path, value<double>(Prop::LineWidth));
}

void Pipe::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_PATH_OBJECT(hmi::Pipe, "pipe", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Pipe"),
                         hmi::ObjectCategory::ProcessSymbols, ":/icons/pipe.svg", OrthogonalPoints)
