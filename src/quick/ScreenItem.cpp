#include "quick/ScreenItem.h"

#include "core/model/DocumentNotifier.h"
#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "quick/ObjectItem.h"

namespace hmi
{

ScreenItem::ScreenItem(QQuickItem* parent)
    : QQuickItem(parent)
{
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
    item->setEditing(m_editing);
    item->setRenderScale(renderScaleFor(*item));
    item->setGraphics(m_graphics);
    item->setScreens(m_screens);
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
    setSize(m_screen ? m_screen->size() : QSizeF());
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

void ScreenItem::setRenderScale(double scale, const QRectF& sharpArea)
{
    m_renderScale = scale;
    m_sharpArea = sharpArea;
    for (ObjectItem* item : std::as_const(m_items))
        item->setRenderScale(renderScaleFor(*item));
}

void ScreenItem::setEditing(bool editing)
{
    m_editing = editing;
    for (ObjectItem* item : std::as_const(m_items))
        item->setEditing(editing);
}

void ScreenItem::setScreens(const IScreenSource* screens)
{
    m_screens = screens;
    for (ObjectItem* item : std::as_const(m_items))
        item->setScreens(screens);
}

void ScreenItem::setGraphics(const IGraphicProvider* graphics)
{
    m_graphics = graphics;
    for (ObjectItem* item : std::as_const(m_items))
        item->setGraphics(graphics);
}

double ScreenItem::renderScaleFor(const ObjectItem& item) const
{
    if (m_sharpArea.isNull() || m_renderScale <= 1.0)
        return m_renderScale;
    const QRectF bounds = item.mapRectToItem(this, item.boundingRect());
    return bounds.intersects(m_sharpArea) ? m_renderScale : 1.0;
}

void ScreenItem::showPreview(ObjectId id, const QRectF& sceneGeometry, double rotation)
{
    if (ObjectItem* item = m_items.value(id))
    {
        item->showPreview(sceneGeometry, rotation);
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

} // namespace hmi
