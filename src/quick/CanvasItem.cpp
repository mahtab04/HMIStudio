#include "quick/CanvasItem.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "quick/ScreenItem.h"
#include "quick/overlays/GridItem.h"
#include "quick/overlays/GuideOverlay.h"
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

    // Created in stacking order: background and grid, objects, guides,
    // selection.
    m_grid = new GridItem(this);
    m_screenItem = new ScreenItem(this);
    m_guideOverlay = new GuideOverlay(this);
    m_overlay = new SelectionOverlay(this);

    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(kZoomSettleMs);
    connect(&m_settleTimer, &QTimer::timeout, this, &CanvasItem::onZoomSettled);
    connect(&m_recognizer, &GestureRecognizer::gesture, this, &CanvasItem::forwardGesture);
    connect(m_screenItem, &ScreenItem::contentChanged, this,
            [this]
            {
                updateGrid();
                refreshOverlay();
            });
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
    m_selectionSet.clear();
    m_rubberBand.reset();
    m_guides.clear();
    m_screenItem->setScreen(screen, notifier);
    m_needsFit = true;
    if (width() > 0 && height() > 0)
        fitScreen();
    else
        applyView();
}

void CanvasItem::setGraphics(const IGraphicProvider* graphics)
{
    m_screenItem->setGraphics(graphics);
}

void CanvasItem::setSelection(std::vector<ObjectId> ids)
{
    m_selection = std::move(ids);
    m_selectionSet = QSet<ObjectId>(m_selection.begin(), m_selection.end());
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
    m_fitted = true;
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
    m_fitted = false;
    setView(zoom, viewPos - scene * zoom);
}

void CanvasItem::panBy(QPointF viewDelta)
{
    m_fitted = false;
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
    // After zooming, and after panning (new objects come into view), the
    // textures are re-rendered for the new view once it stops changing.
    m_settleTimer.start();
}

void CanvasItem::applyView()
{
    m_screenItem->setPosition(m_pan);
    m_screenItem->setScale(m_zoom);
    updateGrid();
    refreshOverlay();
    if (m_settledZoom <= 0.0)
        onZoomSettled();
    Q_EMIT viewChanged();
}

void CanvasItem::onZoomSettled()
{
    const double dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    m_settledZoom = m_zoom;
    const QRectF sharp =
        QRectF(mapViewToScene(QPointF(-kSharpMargin, -kSharpMargin)),
               mapViewToScene(QPointF(width() + kSharpMargin, height() + kSharpMargin)));
    m_screenItem->setRenderScale(m_zoom * dpr, sharp);
    Q_EMIT zoomSettled();
}

void CanvasItem::applyAppearance()
{
    m_overlay->setColors(m_selectionColor, m_handleFillColor, m_rubberBandColor);
    m_guideOverlay->setColor(m_guideColor);
    updateGrid();
}

void CanvasItem::updateGrid()
{
    const QSizeF screenSize = m_screen ? m_screen->size() : QSizeF();
    const QColor background = m_screen ? m_screen->backgroundColor() : QColor(Qt::transparent);
    m_grid->setGrid(QRectF(m_pan, screenSize * m_zoom), m_zoom, m_gridSpacing, m_gridColor,
                    background);
}

void CanvasItem::refreshOverlay()
{
    std::vector<SelectionOverlay::Frame> frames;
    std::vector<QPointF> handles;
    std::optional<SelectionOverlay::RotationHandle> rotationHandle;
    QRectF viewBounds;
    QRectF sceneBounds;
    QSizeF selectionSize;
    double selectionRotation = 0.0;
    if (m_screen && !m_selectionSet.isEmpty())
    {
        // One pass over the screen: looking each id up would be quadratic.
        for (const auto& owned : m_screen->objects())
        {
            const HmiObject* object = owned.get();
            const ObjectId id = object->id();
            if (!m_selectionSet.contains(id))
                continue;
            const Preview preview = m_preview.value(id);
            const QRectF geometry = preview.geometry.value_or(object->geometry());
            const double rotation = preview.rotation.value_or(object->rotation());
            frames.push_back({.centre = mapSceneToView(geometry.center()),
                              .size = geometry.size() * m_zoom,
                              .rotation = rotation});

            const QRectF sceneRect = SelectionGeometry::frame(geometry, rotation).boundingRect();
            sceneBounds = sceneBounds.isNull() ? sceneRect : sceneBounds.united(sceneRect);
            selectionSize = geometry.size();
            selectionRotation = rotation;

            if (m_selection.size() == 1 && !object->isLocked())
            {
                for (const QPointF& h : SelectionGeometry::handles(geometry, rotation))
                    handles.push_back(mapSceneToView(h));
                const double offset = m_settings.rotationHandleOffset / m_zoom;
                rotationHandle = SelectionOverlay::RotationHandle{
                    .anchor =
                        mapSceneToView(SelectionGeometry::rotationHandle(geometry, rotation, 0.0)),
                    .handle = mapSceneToView(
                        SelectionGeometry::rotationHandle(geometry, rotation, offset))};
            }
        }
    }
    if (frames.size() > 1)
    {
        selectionSize = sceneBounds.size();
        selectionRotation = 0.0;
    }
    if (frames.size() > kMaxSelectionFrames)
    {
        frames = {{.centre = mapSceneToView(sceneBounds.center()),
                   .size = sceneBounds.size() * m_zoom,
                   .rotation = 0.0}};
    }
    if (!sceneBounds.isNull())
        viewBounds = QRectF(mapSceneToView(sceneBounds.topLeft()),
                            mapSceneToView(sceneBounds.bottomRight()));

    std::optional<QRectF> band;
    if (m_rubberBand)
        band = QRectF(mapSceneToView(m_rubberBand->topLeft()),
                      mapSceneToView(m_rubberBand->bottomRight()));
    m_overlay->setShapes(std::move(frames), std::move(handles), rotationHandle, band);
    std::vector<QPointF> path;
    path.reserve(m_pathPreview.size());
    for (const QPointF& p : m_pathPreview)
        path.push_back(mapSceneToView(p));
    m_overlay->setPath(std::move(path));

    std::vector<QLineF> guides;
    guides.reserve(m_guides.size());
    for (const QLineF& line : m_guides)
        guides.emplace_back(mapSceneToView(line.p1()), mapSceneToView(line.p2()));
    m_guideOverlay->setGuides(std::move(guides));

    if (viewBounds != m_selectionBounds || selectionSize != m_selectionSize ||
        selectionRotation != m_selectionRotation || previewActive() != m_previewWasActive)
    {
        m_selectionBounds = viewBounds;
        m_selectionSize = selectionSize;
        m_selectionRotation = selectionRotation;
        m_previewWasActive = previewActive();
        Q_EMIT selectionGeometryChanged();
    }
}

void CanvasItem::showPreview(ObjectId id)
{
    const HmiObject* object = m_screen ? m_screen->object(id) : nullptr;
    if (!object)
        return;
    const Preview preview = m_preview.value(id);
    m_screenItem->showPreview(id, preview.geometry.value_or(object->geometry()),
                              preview.rotation.value_or(object->rotation()));
    refreshOverlay();
}

void CanvasItem::setPreviewGeometry(ObjectId id, const QRectF& sceneGeometry)
{
    m_preview[id].geometry = sceneGeometry;
    showPreview(id);
}

void CanvasItem::setPreviewRotation(ObjectId id, double rotation)
{
    m_preview[id].rotation = rotation;
    showPreview(id);
}

void CanvasItem::clearPreview()
{
    m_preview.clear();
    m_screenItem->clearPreview();
    refreshOverlay();
}

void CanvasItem::setPathPreview(const std::vector<QPointF>& scenePoints)
{
    m_pathPreview = scenePoints;
    refreshOverlay();
}

void CanvasItem::setGuides(std::vector<QLineF> sceneLines)
{
    if (sceneLines == m_guides)
        return;
    m_guides = std::move(sceneLines);
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

void CanvasItem::setGuideColor(const QColor& color)
{
    if (color == m_guideColor)
        return;
    m_guideColor = color;
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
    m_guideOverlay->setSize(newGeometry.size());
    m_overlay->setSize(newGeometry.size());
    // A fitted screen stays fitted as panels open, close or are dragged,
    // until the user zooms or pans.
    if ((m_needsFit || m_fitted) && newGeometry.width() > 0 && newGeometry.height() > 0)
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
