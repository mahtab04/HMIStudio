#include "quick/RuntimeView.h"

#include "core/model/Screen.h"
#include "quick/ScreenItem.h"

#include <QQuickWindow>
#include <QSGSimpleRectNode>

#include <algorithm>

namespace hmi
{

namespace
{
const QColor kSurround(0x10, 0x12, 0x15);
} // namespace

RuntimeView::RuntimeView(QQuickItem* parent)
    : QQuickItem(parent)
    , m_screenItem(new ScreenItem(this))
{
    setFlag(ItemHasContents);
    setClip(true);
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
    m_screen = screen;
    m_screenItem->setScreen(screen, notifier);
    layout();
    update();
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
