#pragma once

#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QSizeF>

#include <optional>
#include <vector>

namespace hmi
{

// Selection frames, resize handles and the rubber band, in view coordinates
// so handles keep a constant on-screen size at every zoom. Owned and fed by
// CanvasItem, which computes the shapes from the model and any tool preview.
// Drawn with rectangle and transform nodes so every scene graph backend,
// including the software renderer, shows it.
class SelectionOverlay : public QQuickItem
{
    Q_OBJECT

public:
    // A selection frame: an object's rectangle in view coordinates, rotated
    // by `rotation` degrees about its centre.
    struct Frame
    {
        QPointF centre;
        QSizeF size;
        double rotation = 0.0;
    };

    // Drawn side of a handle. The touch hit area is larger (InputSettings).
    static constexpr double kHandleSize = 8.0;

    explicit SelectionOverlay(QQuickItem* parent = nullptr);

    void setShapes(std::vector<Frame> frames, std::vector<QPointF> handles,
                   std::optional<QRectF> rubberBand);
    void setColors(const QColor& accent, const QColor& handleFill, const QColor& rubberFill);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    std::vector<Frame> m_frames;
    std::vector<QPointF> m_handles;
    std::optional<QRectF> m_rubberBand;
    QColor m_accent{0x4C, 0x8D, 0xFF};
    QColor m_handleFill{Qt::white};
    QColor m_rubberFill{0x4C, 0x8D, 0xFF, 0x30};
};

} // namespace hmi
