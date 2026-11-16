#pragma once

#include <QColor>
#include <QQuickItem>
#include <QRectF>

namespace hmi
{

// The screen's background and grid dots, below the objects, drawn in view
// coordinates so dots stay one pixel at every zoom. When the grid would be
// denser than kMinimumViewSpacing, every second row and column is dropped
// until it is not. Owned and configured by CanvasItem.
class GridItem : public QQuickItem
{
    Q_OBJECT

public:
    static constexpr double kMinimumViewSpacing = 8.0;

    explicit GridItem(QQuickItem* parent = nullptr);

    // `screenViewRect` is the screen's rectangle in view coordinates.
    void setGrid(const QRectF& screenViewRect, double zoom, double spacing, const QColor& color,
                 const QColor& background);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    QRectF m_screenViewRect;
    double m_zoom = 1.0;
    double m_spacing = 8.0;
    QColor m_color;
    QColor m_background;
};

} // namespace hmi
