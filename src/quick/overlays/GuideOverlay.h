#pragma once

#include <QColor>
#include <QLineF>
#include <QQuickItem>

#include <vector>

namespace hmi
{

// Snap guides: thin horizontal and vertical lines shown while a move or
// resize snaps to another object or the screen. In view coordinates, one
// pixel wide at every zoom. Owned and fed by CanvasItem; drawn with
// rectangle nodes so the software renderer shows them too.
class GuideOverlay : public QQuickItem
{
    Q_OBJECT

public:
    explicit GuideOverlay(QQuickItem* parent = nullptr);

    // Lines in view coordinates. Lines that are neither horizontal nor
    // vertical are ignored.
    void setGuides(std::vector<QLineF> viewLines);
    [[nodiscard]] const std::vector<QLineF>& guides() const noexcept { return m_lines; }
    void setColor(const QColor& color);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    std::vector<QLineF> m_lines;
    QColor m_color{0xFF, 0x3D, 0x7F};
};

} // namespace hmi
