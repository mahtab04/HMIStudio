#include "quick/ScreenItem.h"

#include "core/model/DocumentNotifier.h"
#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "quick/ObjectItem.h"

#include <QSGSimpleRectNode>

namespace hmi
{

ScreenItem::ScreenItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setTransformOrigin(QQuickItem::TopLeft);
    setAcceptedMouseButtons(Qt::NoButton);
}

ScreenItem::~ScreenItem() = default;

void ScreenItem::setScreen(const Screen* screen, DocumentNotifier* notifier)
{
    if (m_notifier)
        disconnect(m_notifier, nullptr, this, nullptr);
    m_screen = screen;
    m_notifier = notifier;

    if (m_notifier && m_screen)
    {
        const ScreenId id = m_screen->id();
        connect(m_notifier, &DocumentNotifier::objectAdded, this,
                [this, id](ScreenId changed, ObjectId object, int)
                {
                    if (changed == id)
                        addItem(object);
                });
        connect(m_notifier, &DocumentNotifier::objectRemoved, this,
                [this, id](ScreenId changed, ObjectId object)
                {
                    if (changed == id)
                        removeItem(object);
                });
        connect(m_notifier, &DocumentNotifier::objectsReordered, this,
                [this, id](ScreenId changed)
                {
                    if (changed == id)
                        syncZOrder();
                });
        connect(m_notifier, &DocumentNotifier::screenChanged, this,
                [this, id](ScreenId changed)
                {
                    if (changed == id)
                        syncScreen();
                });
        connect(m_notifier, &DocumentNotifier::propertyChanged, this,
                &ScreenItem::onPropertyChanged);
    }
    rebuild();
}

void ScreenItem::rebuild()
{
    for (ObjectItem* item : std::as_const(m_items))
    {
        item->detach();
        item->setParentItem(nullptr);
        item->deleteLater();
    }
    m_items.clear();
    m_previewed.clear();

    if (m_screen)
    {
        for (const auto& object : m_screen->objects())
            addItem(object->id());
    }
    syncScreen();
}

void ScreenItem::addItem(ObjectId id)
{
    const HmiObject* object = m_screen ? m_screen->object(id) : nullptr;
    if (!object || m_items.contains(id))
        return;
    auto* item = new ObjectItem(object, this);
    item->setRenderScale(m_renderScale);
    m_items.insert(id, item);
    syncZOrder();
}

void ScreenItem::removeItem(ObjectId id)
{
    ObjectItem* item = m_items.take(id);
    if (!item)
        return;
    // The object may be destroyed right after this signal; the item must not
    // paint it again.
    item->detach();
    item->setParentItem(nullptr);
    item->deleteLater();
    m_previewed.removeAll(id);
    Q_EMIT contentChanged();
}

void ScreenItem::syncZOrder()
{
    if (!m_screen)
        return;
    int z = 0;
    for (const auto& object : m_screen->objects())
    {
        if (ObjectItem* item = m_items.value(object->id()))
            item->setZ(z++);
    }
    Q_EMIT contentChanged();
}

void ScreenItem::syncScreen()
{
    if (m_screen)
    {
        setSize(m_screen->size());
        m_background = m_screen->backgroundColor();
    }
    else
    {
        setSize(QSizeF());
    }
    update();
    Q_EMIT contentChanged();
}

void ScreenItem::onPropertyChanged(ObjectId id)
{
    if (ObjectItem* item = m_items.value(id))
    {
        item->syncFromModel();
        Q_EMIT contentChanged();
    }
}

void ScreenItem::setRenderScale(double scale)
{
    m_renderScale = scale;
    for (ObjectItem* item : std::as_const(m_items))
        item->setRenderScale(scale);
}

void ScreenItem::showPreview(ObjectId id, const QRectF& sceneGeometry)
{
    if (ObjectItem* item = m_items.value(id))
    {
        item->showPreview(sceneGeometry);
        if (!m_previewed.contains(id))
            m_previewed.append(id);
    }
}

void ScreenItem::clearPreview()
{
    for (const ObjectId& id : std::as_const(m_previewed))
    {
        if (ObjectItem* item = m_items.value(id))
            item->syncFromModel();
    }
    m_previewed.clear();
}

QSGNode* ScreenItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGSimpleRectNode*>(oldNode);
    if (!node)
        node = new QSGSimpleRectNode();
    node->setRect(boundingRect());
    node->setColor(m_background);
    return node;
}

} // namespace hmi
