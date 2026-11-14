#include "quick/ObjectItem.h"

#include "core/model/HmiObject.h"
#include "core/model/PaintContext.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

// Hidden objects stay visible in the editor, dimmed, so they can be selected.
constexpr double kHiddenOpacity = 0.35;

} // namespace

ObjectItem::ObjectItem(const HmiObject* object, QQuickItem* parent)
    : QQuickPaintedItem(parent)
    , m_object(object)
{
    setAntialiasing(true);
    setTransformOrigin(QQuickItem::Center);
    setAcceptedMouseButtons(Qt::NoButton);
    syncFromModel();
}

void ObjectItem::syncFromModel()
{
    if (!m_object)
        return;
    const QRectF g = m_object->geometry();
    setPosition(g.topLeft());
    setSize(g.size());
    setRotation(m_object->rotation());
    setOpacity(m_object->isVisible() ? 1.0 : kHiddenOpacity);
    updateTextureSize();
    update();
}

void ObjectItem::showPreview(const QRectF& sceneGeometry)
{
    setPosition(sceneGeometry.topLeft());
    setSize(sceneGeometry.size());
    updateTextureSize();
    update();
}

void ObjectItem::setRenderScale(double scale)
{
    if (qFuzzyCompare(scale, m_renderScale))
        return;
    m_renderScale = scale;
    updateTextureSize();
    update();
}

void ObjectItem::updateTextureSize()
{
    const auto side = [this](double length)
    {
        return std::clamp(static_cast<int>(std::ceil(length * m_renderScale)), 1, kMaxTextureSide);
    };
    setTextureSize(QSize(side(width()), side(height())));
}

void ObjectItem::paint(QPainter* painter)
{
    if (!m_object)
        return;

    // During a resize preview the item is larger or smaller than the model;
    // stretch the model's drawing to fill it.
    const QSizeF modelSize = m_object->geometry().size();
    if (modelSize.width() > 0.0 && modelSize.height() > 0.0 &&
        (!qFuzzyCompare(modelSize.width(), width()) ||
         !qFuzzyCompare(modelSize.height(), height())))
    {
        painter->scale(width() / modelSize.width(), height() / modelSize.height());
    }

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    m_object->paint(*painter, PaintContext{.zoom = m_renderScale});
}

} // namespace hmi
