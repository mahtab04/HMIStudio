#include "quick/overlays/RulerItem.h"

#include "quick/CanvasItem.h"

#include <QPainter>

#include <array>
#include <cmath>

namespace hmi
{

namespace
{

constexpr int kMinorTicksPerLabel = 5;
constexpr double kMajorTickLength = 0.5; // fraction of the ruler's thickness
constexpr double kMinorTickLength = 0.25;
constexpr double kLabelInset = 3.0;

} // namespace

RulerItem::RulerItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
    connect(this, &RulerItem::appearanceChanged, this, [this] { update(); });
}

CanvasItem* RulerItem::canvas() const
{
    return m_canvas;
}

void RulerItem::setCanvas(CanvasItem* canvas)
{
    if (canvas == m_canvas)
        return;
    disconnect(m_viewConnection);
    m_canvas = canvas;
    if (m_canvas)
        m_viewConnection = connect(m_canvas, &CanvasItem::viewChanged, this, [this] { update(); });
    update();
    Q_EMIT canvasChanged();
}

void RulerItem::setOrientation(Qt::Orientation orientation)
{
    if (orientation == m_orientation)
        return;
    m_orientation = orientation;
    update();
    Q_EMIT orientationChanged();
}

double RulerItem::labelStep(double zoom)
{
    if (zoom <= 0.0)
        return 1.0;
    const double minimumScene = kMinimumLabelSpacing / zoom;
    double magnitude = std::pow(10.0, std::floor(std::log10(minimumScene)));
    for (;;)
    {
        for (const double factor : std::array{1.0, 2.0, 5.0})
        {
            if (factor * magnitude >= minimumScene)
                return factor * magnitude;
        }
        magnitude *= 10.0;
    }
}

void RulerItem::paint(QPainter* painter)
{
    painter->fillRect(boundingRect(), m_background);
    if (!m_canvas || m_canvas->zoom() <= 0.0)
        return;

    const bool horizontal = m_orientation == Qt::Horizontal;
    const double length = horizontal ? width() : height();
    const double thickness = horizontal ? height() : width();

    // Ruler coordinate -> canvas view coordinate -> scene coordinate.
    const QPointF origin = mapToItem(m_canvas, QPointF(0, 0));
    const double viewOffset = horizontal ? origin.x() : origin.y();
    const auto toScene = [&](double rulerPos)
    {
        const QPointF view =
            horizontal ? QPointF(rulerPos + viewOffset, 0) : QPointF(0, rulerPos + viewOffset);
        const QPointF scene = m_canvas->mapViewToScene(view);
        return horizontal ? scene.x() : scene.y();
    };
    const double zoom = m_canvas->zoom();
    const double step = labelStep(zoom);
    const double minor = step / kMinorTicksPerLabel;
    const double sceneStart = std::floor(toScene(0.0) / minor) * minor;
    const double sceneEnd = toScene(length);

    painter->setPen(QPen(m_tickColor, 1.0));
    painter->setFont(m_font);
    for (double s = sceneStart; s <= sceneEnd; s += minor)
    {
        const double pos = std::round((s - toScene(0.0)) * zoom) + 0.5;
        const long index = std::lround(s / minor);
        const bool major = index % kMinorTicksPerLabel == 0;
        const double tick = thickness * (major ? kMajorTickLength : kMinorTickLength);
        if (horizontal)
            painter->drawLine(QPointF(pos, thickness - tick), QPointF(pos, thickness));
        else
            painter->drawLine(QPointF(thickness - tick, pos), QPointF(thickness, pos));

        if (!major)
            continue;
        const QString label = QString::number(std::lround(s));
        painter->setPen(m_textColor);
        if (horizontal)
        {
            painter->drawText(QPointF(pos + kLabelInset, thickness * kMajorTickLength), label);
        }
        else
        {
            painter->save();
            painter->translate(thickness * kMajorTickLength, pos - kLabelInset);
            painter->rotate(-90);
            painter->drawText(QPointF(0, 0), label);
            painter->restore();
        }
        painter->setPen(QPen(m_tickColor, 1.0));
    }
}

} // namespace hmi
