#pragma once

#include <QColor>
#include <QQuickItem>
#include <QRectF>

namespace hmi
{

// Grid lines over the screen area, drawn with scene-graph geometry in view
// coordinates so lines stay one pixel wide at every zoom. When the grid would
// be denser than kMinimumViewSpacing, every second line is dropped until it
// is not. Owned and configured by CanvasItem.
class GridItem : public QQuickItem
{
    Q_OBJECT

public:
    static constexpr double kMinimumViewSpacing = 8.0;

    explicit GridItem(QQuickItem* parent = nullptr);

    // `screenViewRect` is the screen's rectangle in view coordinates.
    void setGrid(const QRectF& screenViewRect, double zoom, double spacing, const QColor& color);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    QRectF m_screenViewRect;
    double m_zoom = 1.0;
    double m_spacing = 8.0;
    QColor m_color;
};

} // namespace hmi
