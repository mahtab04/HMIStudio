#include "quick/RuntimeView.h"

#include "core/model/Screen.h"
#include "quick/ScreenItem.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSGSimpleRectNode>
#include <QTouchEvent>

#include <algorithm>

namespace hmi
{

namespace
{
const QColor kSurround(0x10, 0x12, 0x15);
} // namespace

RuntimeView::RuntimeView(QQuickItem* parent)
    : QQuickItem(parent)
    , m_recognizer(m_settings, this)
    , m_screenItem(new ScreenItem(this))
{
    setFlag(ItemHasContents);
    m_screenItem->setEditing(false);
    setClip(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
    setActiveFocusOnTab(true);
    m_recognizer.setImmediateGestures(true);
    const auto observeWindow = [this](QQuickWindow* target)
    {
        disconnect(m_activationConnection);
        if (target)
            m_activationConnection = connect(target, &QWindow::activeChanged, this,
                                             [this, target]
                                             {
                                                 if (!target->isActive())
                                                 {
                                                     m_recognizer.cancel();
                                                     Q_EMIT inputCancelled();
                                                 }
                                             });
    };
    connect(this, &QQuickItem::windowChanged, this, observeWindow);
    observeWindow(window());
    connect(&m_recognizer, &GestureRecognizer::gesture, this,
            [this](PointerGesture g)
            {
                g.scenePos = m_screenItem->mapFromItem(this, g.viewPos);
                g.startScenePos = m_screenItem->mapFromItem(this, g.startViewPos);
                Q_EMIT gesture(g);
            });
    connect(m_screenItem, &ScreenItem::contentChanged, this,
            [this]
            {
                layout();
                update();
            });
}

RuntimeView::~RuntimeView() = default;

void RuntimeView::setScreen(const Screen* screen, DocumentNotifier* notifier)
{
    m_recognizer.cancel();
    Q_EMIT inputCancelled();
    m_screen = screen;
    m_screenItem->setScreen(screen, notifier);
    layout();
    update();
}

void RuntimeView::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();
    event->setAccepted(m_recognizer.mousePress(*event));
}
void RuntimeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    mousePressEvent(event);
}
void RuntimeView::mouseMoveEvent(QMouseEvent* event)
{
    event->setAccepted(m_recognizer.mouseMove(*event));
}
void RuntimeView::mouseReleaseEvent(QMouseEvent* event)
{
    event->setAccepted(m_recognizer.mouseRelease(*event));
}
void RuntimeView::mouseUngrabEvent()
{
    m_recognizer.cancel();
    Q_EMIT inputCancelled();
}
void RuntimeView::touchEvent(QTouchEvent* event)
{
    if (event->type() == QEvent::TouchBegin)
        forceActiveFocus();
    event->setAccepted(m_recognizer.touch(*event));
}
void RuntimeView::touchUngrabEvent()
{
    mouseUngrabEvent();
}
void RuntimeView::keyPressEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
        Q_EMIT keyInput(event->key(), event->modifiers(), false);
    event->accept();
}
void RuntimeView::keyReleaseEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
        Q_EMIT keyInput(event->key(), event->modifiers(), true);
    event->accept();
}
void RuntimeView::focusOutEvent(QFocusEvent* event)
{
    m_recognizer.cancel();
    Q_EMIT inputCancelled();
    QQuickItem::focusOutEvent(event);
}

void RuntimeView::setGraphics(const IGraphicProvider* graphics)
{
    m_screenItem->setGraphics(graphics);
}

void RuntimeView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    layout();
    update();
}

void RuntimeView::layout()
{
    const QSizeF size = m_screen ? m_screen->size() : QSizeF();
    if (size.isEmpty() || width() <= 0 || height() <= 0)
        return;
    m_zoom = std::min(width() / size.width(), height() / size.height());
    m_screenItem->setScale(m_zoom);
    m_screenItem->setPosition(
        {(width() - size.width() * m_zoom) / 2, (height() - size.height() * m_zoom) / 2});
    const double dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    m_screenItem->setRenderScale(m_zoom * dpr, QRectF(QPointF(), size));
    Q_EMIT viewChanged();
}

QSGNode* RuntimeView::updatePaintNode(QSGNode* old, UpdatePaintNodeData*)
{
    // The surround, then the screen's background under the objects.
    auto* surround = static_cast<QSGSimpleRectNode*>(old);
    if (!surround)
    {
        surround = new QSGSimpleRectNode;
        surround->appendChildNode(new QSGSimpleRectNode);
    }
    surround->setRect(boundingRect());
    surround->setColor(kSurround);
    auto* background = static_cast<QSGSimpleRectNode*>(surround->firstChild());
    if (m_screen)
    {
        background->setRect(QRectF(m_screenItem->position(), m_screen->size() * m_zoom));
        background->setColor(m_screen->backgroundColor());
    }
    else
    {
        background->setRect(QRectF());
    }
    return surround;
}

} // namespace hmi
