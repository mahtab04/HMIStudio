#include "core/objects/PaintHelpers.h"

#include "core/model/HmiObject.h"
#include "core/property/Prop.h"

#include <QFontMetricsF>
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
    painter.setBrush(fill);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.restore();
}

void drawRaisedFace(QPainter& painter, const QRectF& rect, const QColor& fill, const QColor& line,
                    double radius)
{
    painter.save();
    const QRectF r = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient shade(r.topLeft(), r.bottomLeft());
    shade.setColorAt(0.0, fill.lighter(118));
    shade.setColorAt(0.48, fill.lighter(104));
    shade.setColorAt(0.52, fill);
    shade.setColorAt(1.0, fill.darker(114));
    painter.setPen(QPen(line, 1.0));
    painter.setBrush(shade);
    painter.drawRoundedRect(r, radius, radius);
    // A thin highlight inside the top edge.
    QColor highlight(Qt::white);
    highlight.setAlpha(130);
    painter.setPen(QPen(highlight, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r.adjusted(1, 1, -1, -1), std::max(0.0, radius - 1),
                            std::max(0.0, radius - 1));
    painter.restore();
}

void drawScale(QPainter& painter, const QLineF& axis, QPointF outward, double tickLength,
               double minimum, double maximum, const QColor& color, const QFont& font, bool labels)
{
    painter.save();
    painter.setPen(QPen(color, 1.0));
    painter.setFont(font);
    constexpr int kMajor = 5;
    constexpr int kMinor = 2; // minor ticks between two numbers
    const QFontMetricsF metrics(font);
    for (int i = 0; i <= kMajor * kMinor; ++i)
    {
        const double t = double(i) / (kMajor * kMinor);
        const QPointF at = axis.pointAt(t);
        const bool major = i % kMinor == 0;
        painter.drawLine(at, at + outward * (major ? tickLength : tickLength * 0.55));
        if (major && labels)
        {
            const double number = minimum + (maximum - minimum) * t;
            const QString text = QString::number(number, 'g', 4);
            const QSizeF box(metrics.horizontalAdvance(text) + 2, metrics.height());
            // Place the label centred one tick length beyond the tick.
            const QPointF centre =
                at + outward * (tickLength + 2) +
                QPointF(outward.x() * box.width() / 2.0, outward.y() * box.height() / 2.0);
            painter.drawText(QRectF(centre - QPointF(box.width() / 2.0, box.height() / 2.0), box),
                             Qt::AlignCenter, text);
        }
    }
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
