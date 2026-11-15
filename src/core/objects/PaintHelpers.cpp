#include "core/objects/PaintHelpers.h"

#include "core/model/HmiObject.h"
#include "core/property/Prop.h"

#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>

namespace hmi::PaintHelpers
{

QPen linePen(const HmiObject& object)
{
    const double width = object.value<double>(Prop::LineWidth);
    if (width <= 0.0)
        return Qt::NoPen;
    QPen pen(object.value<QColor>(Prop::LineColor), width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}

QPolygonF toLocal(const PointList& relative, QSizeF size)
{
    QPolygonF local;
    local.reserve(relative.size());
    for (const QPointF& p : relative)
        local.append(QPointF(p.x() * size.width(), p.y() * size.height()));
    return local;
}

QPainterPath strokeArea(const QPainterPath& path, double width)
{
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(width, 1.0));
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    return stroker.createStroke(path);
}

void drawText(QPainter& painter, const HmiObject& object, const QRectF& rect, int flags,
              const QString& text)
{
    painter.save();
    painter.setFont(object.value<QFont>(Prop::Font));
    painter.setPen(object.value<QColor>(Prop::TextColor));
    painter.setClipRect(rect, Qt::IntersectClip);
    painter.drawText(rect, flags, text);
    painter.restore();
}

void drawFace(QPainter& painter, const QRectF& rect, const QColor& fill, const QColor& line,
              double radius)
{
    painter.save();
    painter.setPen(QPen(line, 1.0));
    QLinearGradient shade(rect.topLeft(), rect.bottomLeft());
    shade.setColorAt(0.0, fill.lighter(108));
    shade.setColorAt(1.0, fill.darker(108));
    painter.setBrush(shade);
    const QRectF r = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(r, radius, radius);
    painter.restore();
}

void drawPlaceholder(QPainter& painter, const QRectF& rect, const QString& title,
                     const QColor& background)
{
    painter.save();
    painter.setBrush(background);
    QPen pen(QColor(0x6B, 0x72, 0x80), 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    painter.setPen(QColor(0x4B, 0x52, 0x5C));
    painter.drawText(rect.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignTop, title);
    painter.restore();
}

QSizeF scaleToHeight(QPainter& painter, const HmiObject& object)
{
    const QSizeF size = object.geometry().size();
    const PropertyDescriptor* d = findDescriptor(object.descriptors(), Prop::Height);
    const double* design = d ? std::get_if<double>(&d->defaultValue) : nullptr;
    if (!design || *design <= 0.0 || size.height() <= 0.0)
        return size;
    const double s = size.height() / *design;
    painter.scale(s, s);
    return {size.width() / s, *design};
}

double fraction(double value, double minimum, double maximum)
{
    if (maximum <= minimum)
        return 0.0;
    return std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0);
}

} // namespace hmi::PaintHelpers
