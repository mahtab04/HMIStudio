#pragma once

#include "core/model/Ids.h"
#include "quick/ICanvasView.h"
#include "quick/input/GestureRecognizer.h"
#include "quick/input/InputSettings.h"
#include "quick/input/PointerGesture.h"

#include <QColor>
#include <QHash>
#include <QQuickItem>
#include <QTimer>
#include <qqmlintegration.h>

#include <optional>
#include <vector>

namespace hmi
{

class DocumentNotifier;
class GridItem;
class GuideOverlay;
class Screen;
class ScreenItem;
class SelectionOverlay;

// The editor viewport. Owns zoom and pan, hosts the grid, the ScreenItem
// (scaled and translated), the snap guides and the selection overlay, in that
// stacking order, and is the only item that
// accepts input: mouse, touch, pen, wheel and keys go through the
// GestureRecognizer and leave as `gesture` signals with scene positions
// filled in. What a gesture does is decided by the editor's tools.
//
// View coordinates are this item's pixels; scene coordinates are the
// screen's: view = scene * zoom + panOffset.
class CanvasItem : public QQuickItem, public ICanvasView
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(QPointF panOffset READ panOffset NOTIFY viewChanged)
    Q_PROPERTY(double gridSpacing READ gridSpacing WRITE setGridSpacing NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY appearanceChanged)
    Q_PROPERTY(
        QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor handleFillColor READ handleFillColor WRITE setHandleFillColor NOTIFY
                   appearanceChanged)
    Q_PROPERTY(QColor rubberBandColor READ rubberBandColor WRITE setRubberBandColor NOTIFY
                   appearanceChanged)
    Q_PROPERTY(QColor guideColor READ guideColor WRITE setGuideColor NOTIFY appearanceChanged)
    // Rectangle around the selection frames in view coordinates (empty with
    // no selection), for placing the size label and the touch toolbar.
    Q_PROPERTY(QRectF selectionBounds READ selectionBounds NOTIFY selectionGeometryChanged)
    // True while a tool shows a preview or rubber band.
    Q_PROPERTY(bool previewActive READ previewActive NOTIFY selectionGeometryChanged)

public:
    static constexpr double kMinZoom = 0.1;
    static constexpr double kMaxZoom = 8.0;
    static constexpr int kZoomSettleMs = 150;
    static constexpr double kFitMargin = 24.0;

    explicit CanvasItem(QQuickItem* parent = nullptr);
    ~CanvasItem() override;
    CanvasItem(const CanvasItem&) = delete;
    CanvasItem& operator=(const CanvasItem&) = delete;
    CanvasItem(CanvasItem&&) = delete;
    CanvasItem& operator=(CanvasItem&&) = delete;

    // Shows `screen`; the notifier keeps items in step with the document.
    void setScreen(const Screen* screen, DocumentNotifier* notifier);
    [[nodiscard]] ScreenItem* screenItem() const noexcept { return m_screenItem; }

    void setInputSettings(const InputSettings& settings) { m_settings = settings; }
    void setSelection(std::vector<ObjectId> ids);

    [[nodiscard]] QRectF selectionBounds() const noexcept { return m_selectionBounds; }
    [[nodiscard]] bool previewActive() const noexcept
    {
        return !m_preview.isEmpty() || m_rubberBand.has_value();
    }
    // Size of the single selected object (its preview while a tool runs), or
    // of the rectangle around several; empty with no selection.
    [[nodiscard]] QSizeF selectionSceneSize() const noexcept { return m_selectionSize; }
    // Rotation of the single selected object, 0 otherwise.
    [[nodiscard]] double selectionRotation() const noexcept { return m_selectionRotation; }
    [[nodiscard]] GuideOverlay* guideOverlay() const noexcept { return m_guideOverlay; }

    [[nodiscard]] QPointF panOffset() const noexcept { return m_pan; }
    [[nodiscard]] QPointF mapViewToScene(QPointF viewPos) const;
    [[nodiscard]] QPointF mapSceneToView(QPointF scenePos) const;

    // Zoom so the whole screen fits with a margin, centred.
    Q_INVOKABLE void fitScreen();
    // Sets the zoom keeping the view centre fixed.
    Q_INVOKABLE void setZoomCentered(double zoom);

    // ICanvasView
    [[nodiscard]] double zoom() const override { return m_zoom; }
    void zoomAt(QPointF viewPos, double factor) override;
    void panBy(QPointF viewDelta) override;
    void setPreviewGeometry(ObjectId id, const QRectF& sceneGeometry) override;
    void setPreviewRotation(ObjectId id, double rotation) override;
    void clearPreview() override;
    void setGuides(std::vector<QLineF> sceneLines) override;
    void setRubberBand(const std::optional<QRectF>& sceneRect) override;
    void setToolCursor(Qt::CursorShape shape) override;

    [[nodiscard]] double gridSpacing() const noexcept { return m_gridSpacing; }
    void setGridSpacing(double spacing);
    [[nodiscard]] QColor gridColor() const { return m_gridColor; }
    void setGridColor(const QColor& color);
    [[nodiscard]] QColor selectionColor() const { return m_selectionColor; }
    void setSelectionColor(const QColor& color);
    [[nodiscard]] QColor handleFillColor() const { return m_handleFillColor; }
    void setHandleFillColor(const QColor& color);
    [[nodiscard]] QColor rubberBandColor() const { return m_rubberBandColor; }
    void setRubberBandColor(const QColor& color);
    [[nodiscard]] QColor guideColor() const { return m_guideColor; }
    void setGuideColor(const QColor& color);

Q_SIGNALS:
    void viewChanged();
    void zoomSettled();
    void appearanceChanged();
    void gesture(const hmi::PointerGesture& gesture);
    void keyPressed(int key, Qt::KeyboardModifiers modifiers);
    void pointerMoved(QPointF scenePos);
    void selectionGeometryChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent* event) override;
    void touchUngrabEvent() override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void setView(double zoom, QPointF pan);
    void applyView();
    void applyAppearance();
    void updateGrid();
    void refreshOverlay();
    void showPreview(ObjectId id);
    void onZoomSettled();
    void forwardGesture(PointerGesture g);

    InputSettings m_settings;
    GestureRecognizer m_recognizer;
    ScreenItem* m_screenItem = nullptr;
    GridItem* m_grid = nullptr;
    GuideOverlay* m_guideOverlay = nullptr;
    SelectionOverlay* m_overlay = nullptr;
    QTimer m_settleTimer;

    const Screen* m_screen = nullptr;
    double m_zoom = 1.0;
    double m_settledZoom = 0.0;
    QPointF m_pan;
    bool m_needsFit = true;

    // What a tool shows instead of the model while a gesture runs.
    struct Preview
    {
        std::optional<QRectF> geometry;
        std::optional<double> rotation;
    };

    std::vector<ObjectId> m_selection;
    QHash<ObjectId, Preview> m_preview;
    std::optional<QRectF> m_rubberBand;
    std::vector<QLineF> m_guides;
    QRectF m_selectionBounds;
    QSizeF m_selectionSize;
    double m_selectionRotation = 0.0;
    bool m_previewWasActive = false;

    double m_gridSpacing = 8.0;
    QColor m_gridColor{0, 0, 0, 0x14};
    QColor m_selectionColor{0x4C, 0x8D, 0xFF};
    QColor m_handleFillColor{Qt::white};
    QColor m_rubberBandColor{0x4C, 0x8D, 0xFF, 0x30};
    QColor m_guideColor{0xFF, 0x3D, 0x7F};
};

} // namespace hmi
