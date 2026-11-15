#pragma once

#include <QQuickPaintedItem>

namespace hmi
{

class HmiObject;
class IGraphicProvider;

// Hosts one HmiObject on the canvas. Holds only a non-owning pointer: the
// geometry, rotation and look come from the object every time
// (ARCHITECTURE.md section 7.1). Created and positioned by ScreenItem; never
// accepts input.
class ObjectItem : public QQuickPaintedItem
{
    Q_OBJECT

public:
    // Largest texture side; beyond it the GPU scales the texture
    // (ARCHITECTURE.md section 14.5).
    static constexpr int kMaxTextureSide = 4096;

    explicit ObjectItem(const HmiObject* object, QQuickItem* parent = nullptr);

    [[nodiscard]] const HmiObject* object() const noexcept { return m_object; }

    // Detaches before the object is destroyed; the item then paints nothing.
    void detach() noexcept { m_object = nullptr; }

    // Copies position, size, rotation and visibility from the object.
    void syncFromModel();

    // Shows the object at another geometry and rotation (tool preview). The
    // painting is stretched to fit until the next syncFromModel().
    void showPreview(const QRectF& sceneGeometry, double rotation);

    // Pixels per scene unit the texture is rendered at (zoom times device
    // pixel ratio). Applied when the zoom settles, so the result is sharp.
    void setRenderScale(double scale);

    // Where imported graphics come from; must outlive the item.
    void setGraphics(const IGraphicProvider* graphics);

    void paint(QPainter* painter) override;

private:
    void updateTextureSize();

    const HmiObject* m_object = nullptr;
    double m_renderScale = 1.0;
    const IGraphicProvider* m_graphics = nullptr;
};

} // namespace hmi
