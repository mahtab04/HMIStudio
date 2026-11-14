#include "quick/CanvasItem.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "quick/ScreenItem.h"
#include "quick/overlays/GridItem.h"
#include "quick/overlays/SelectionGeometry.h"
#include "quick/overlays/SelectionOverlay.h"

#include <QCursor>
#include <QQuickWindow>

#include <algorithm>

namespace hmi
{

CanvasItem::CanvasItem(QQuickItem* parent)
    : QQuickItem(parent)
    , m_recognizer(m_settings)
{
    setFlag(ItemIsFocusScope);
    setActiveFocusOnTab(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptTouchEvents(true);
    setAcceptHoverEvents(true);
    setClip(true);

    m_screenItem = new ScreenItem(this);
    m_grid = new GridItem(this);
    m_overlay = new SelectionOverlay(this);

    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(kZoomSettleMs);
    connect(&m_settleTimer, &QTimer::timeout, this, &CanvasItem::onZoomSettled);
    connect(&m_recognizer, &GestureRecognizer::gesture, this, &CanvasItem::forwardGesture);
    connect(m_screenItem, &ScreenItem::contentChanged, this, &CanvasItem::refreshOverlay);
    connect(this, &CanvasItem::appearanceChanged, this, &CanvasItem::applyAppearance);

    applyAppearance();
    applyView();
}

CanvasItem::~CanvasItem() = default;

void CanvasItem::setScreen(const Screen* screen, DocumentNotifier* notifier)
{
    m_screen = screen;
    m_preview.clear();
    m_selection.clear();
    m_rubberBand.reset();
    m_screenItem->setScreen(screen, notifier);
    m_needsFit = true;
    if (width() > 0 && height() > 0)
        fitScreen();
    else
        applyView();
}

void CanvasItem::setSelection(std::vector<ObjectId> ids)
{
    m_selection = std::move(ids);
    refreshOverlay();
}

QPointF CanvasItem::mapViewToScene(QPointF viewPos) const
{
    return (viewPos - m_pan) / m_zoom;
}

QPointF CanvasItem::mapSceneToView(QPointF scenePos) const
{
    return scenePos * m_zoom + m_pan;
}

void CanvasItem::fitScreen()
{
    if (!m_screen || width() <= 0 || height() <= 0)
        return;
    m_needsFit = false;
    const QSizeF size = m_screen->size();
    const double zoom = std::clamp(std::min((width() - 2 * kFitMargin) / size.width(),
                                            (height() - 2 * kFitMargin) / size.height()),
                                   kMinZoom, kMaxZoom);
    const QPointF pan((width() - size.width() * zoom) / 2.0,
                      (height() - size.height() * zoom) / 2.0);
    setView(zoom, pan);
}

void CanvasItem::setZoomCentered(double zoom)
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), zoom / m_zoom);
}

void CanvasItem::zoomAt(QPointF viewPos, double factor)
{
    if (factor <= 0.0)
        return;
    const double zoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
    const QPointF scene = mapViewToScene(viewPos);
    setView(zoom, viewPos - scene * zoom);
}

void CanvasItem::panBy(QPointF viewDelta)
{
    setView(m_zoom, m_pan + viewDelta);
}

void CanvasItem::setView(double zoom, QPointF pan)
{
    const bool zoomChanged = !qFuzzyCompare(zoom, m_zoom);
    if (!zoomChanged && pan == m_pan)
        return;
    m_zoom = zoom;
    m_pan = pan;
    applyView();
    if (zoomChanged)
        m_settleTimer.start();
}

void CanvasItem::applyView()
{
    m_screenItem->setPosition(m_pan);
    m_screenItem->setScale(m_zoom);
    const QSizeF screenSize = m_screen ? m_screen->size() : QSizeF();
    m_grid->setGrid(QRectF(m_pan, screenSize * m_zoom), m_zoom, m_gridSpacing, m_gridColor);
    refreshOverlay();
    if (m_settledZoom <= 0.0)
        onZoomSettled();
    Q_EMIT viewChanged();
}

void CanvasItem::onZoomSettled()
{
    const double dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    m_settledZoom = m_zoom;
    m_screenItem->setRenderScale(m_zoom * dpr);
    Q_EMIT zoomSettled();
}

void CanvasItem::applyAppearance()
{
    m_overlay->setColors(m_selectionColor, m_handleFillColor, m_rubberBandColor);
    const QSizeF screenSize = m_screen ? m_screen->size() : QSizeF();
    m_grid->setGrid(QRectF(m_pan, screenSize * m_zoom), m_zoom, m_gridSpacing, m_gridColor);
}

void CanvasItem::refreshOverlay()
{
    std::vector<SelectionOverlay::Frame> frames;
    std::vector<QPointF> handles;
    if (m_screen)
    {
        for (const ObjectId& id : m_selection)
        {
            const HmiObject* object = m_screen->object(id);
            if (!object)
                continue;
            const QRectF geometry = m_preview.value(id, object->geometry());
            frames.push_back({.centre = mapSceneToView(geometry.center()),
                              .size = geometry.size() * m_zoom,
                              .rotation = object->rotation()});

            if (m_selection.size() == 1 && !object->isLocked())
            {
                for (const QPointF& h : SelectionGeometry::handles(geometry, object->rotation()))
                    handles.push_back(mapSceneToView(h));
            }
        }
    }

    std::optional<QRectF> band;
    if (m_rubberBand)
        band = QRectF(mapSceneToView(m_rubberBand->topLeft()),
                      mapSceneToView(m_rubberBand->bottomRight()));
    m_overlay->setShapes(std::move(frames), std::move(handles), band);
}

void CanvasItem::setPreviewGeometry(ObjectId id, const QRectF& sceneGeometry)
{
    m_preview.insert(id, sceneGeometry);
    m_screenItem->showPreview(id, sceneGeometry);
    refreshOverlay();
}

void CanvasItem::clearPreview()
{
    m_preview.clear();
    m_screenItem->clearPreview();
    refreshOverlay();
}

void CanvasItem::setRubberBand(const std::optional<QRectF>& sceneRect)
{
    m_rubberBand = sceneRect;
    refreshOverlay();
}

void CanvasItem::setToolCursor(Qt::CursorShape shape)
{
    setCursor(QCursor(shape));
}

void CanvasItem::setGridSpacing(double spacing)
{
    if (spacing <= 0.0 || qFuzzyCompare(spacing, m_gridSpacing))
        return;
    m_gridSpacing = spacing;
    Q_EMIT appearanceChanged();
}

void CanvasItem::setGridColor(const QColor& color)
{
    if (color == m_gridColor)
        return;
    m_gridColor = color;
    Q_EMIT appearanceChanged();
}

void CanvasItem::setSelectionColor(const QColor& color)
{
    if (color == m_selectionColor)
        return;
    m_selectionColor = color;
    Q_EMIT appearanceChanged();
}

void CanvasItem::setHandleFillColor(const QColor& color)
{
    if (color == m_handleFillColor)
        return;
    m_handleFillColor = color;
    Q_EMIT appearanceChanged();
}

void CanvasItem::setRubberBandColor(const QColor& color)
{
    if (color == m_rubberBandColor)
        return;
    m_rubberBandColor = color;
    Q_EMIT appearanceChanged();
}

void CanvasItem::forwardGesture(PointerGesture g)
{
    g.scenePos = mapViewToScene(g.viewPos);
    g.startScenePos = mapViewToScene(g.startViewPos);
    Q_EMIT gesture(g);
}

void CanvasItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    m_grid->setSize(newGeometry.size());
    m_overlay->setSize(newGeometry.size());
    if (m_needsFit && newGeometry.width() > 0 && newGeometry.height() > 0)
        fitScreen();
    else
        applyView();
}

void CanvasItem::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    m_recognizer.mousePress(*event);
    event->accept();
}

// Qt delivers press, release, double-click, release; the recognizer detects
// double taps itself, so the double-click is just another press.
void CanvasItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    m_recognizer.mousePress(*event);
    event->accept();
}

void CanvasItem::mouseMoveEvent(QMouseEvent* event)
{
    m_recognizer.mouseMove(*event);
    Q_EMIT pointerMoved(mapViewToScene(event->position()));
    event->accept();
}

void CanvasItem::mouseReleaseEvent(QMouseEvent* event)
{
    m_recognizer.mouseRelease(*event);
    event->accept();
}

void CanvasItem::mouseUngrabEvent()
{
    m_recognizer.cancel();
}

void CanvasItem::touchEvent(QTouchEvent* event)
{
    if (event->type() == QEvent::TouchBegin)
        forceActiveFocus(Qt::OtherFocusReason);
    m_recognizer.touch(*event);
    if (!event->points().isEmpty())
        Q_EMIT pointerMoved(mapViewToScene(event->points().first().position()));
    event->accept();
}

void CanvasItem::touchUngrabEvent()
{
    m_recognizer.cancel();
}

void CanvasItem::wheelEvent(QWheelEvent* event)
{
    m_recognizer.wheel(*event);
    event->accept();
}

void CanvasItem::hoverMoveEvent(QHoverEvent* event)
{
    Q_EMIT pointerMoved(mapViewToScene(event->position()));
    QQuickItem::hoverMoveEvent(event);
}

void CanvasItem::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        m_recognizer.setPanKeyHeld(true);
        event->accept();
        return;
    }
    // Escape first ends a gesture in progress; only an idle Escape reaches
    // the tools.
    if (event->key() == Qt::Key_Escape && m_recognizer.isActive())
    {
        m_recognizer.cancel();
        event->accept();
        return;
    }
    Q_EMIT keyPressed(event->key(), event->modifiers());
    event->accept();
}

void CanvasItem::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
        m_recognizer.setPanKeyHeld(false);
    event->accept();
}

void CanvasItem::focusOutEvent(QFocusEvent* event)
{
    m_recognizer.setPanKeyHeld(false);
    QQuickItem::focusOutEvent(event);
}

} // namespace hmi
