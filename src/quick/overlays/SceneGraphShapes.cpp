#include "quick/overlays/SceneGraphShapes.h"

#include <QQuickWindow>
#include <QSGNode>
#include <QSGRectangleNode>

namespace hmi::SceneGraphShapes
{

void clearChildren(QSGNode* node)
{
    while (QSGNode* child = node->firstChild())
    {
        node->removeChildNode(child);
        delete child;
    }
}

void refreshTransform(QSGTransformNode* itemTransform)
{
    if (itemTransform)
        itemTransform->markDirty(QSGNode::DirtyMatrix);
}

void addRect(QSGNode* parent, QQuickWindow* window, const QRectF& rect, const QColor& color)
{
    QSGRectangleNode* node = window->createRectangleNode();
    node->setRect(rect);
    node->setColor(color);
    parent->appendChildNode(node);
}

void addOutline(QSGNode* parent, QQuickWindow* window, const QRectF& rect, const QColor& color)
{
    const double w = rect.width();
    const double h = rect.height();
    addRect(parent, window, QRectF(rect.left(), rect.top(), w, 1.0), color);
    addRect(parent, window, QRectF(rect.left(), rect.bottom() - 1.0, w, 1.0), color);
    addRect(parent, window, QRectF(rect.left(), rect.top() + 1.0, 1.0, h - 2.0), color);
    addRect(parent, window, QRectF(rect.right() - 1.0, rect.top() + 1.0, 1.0, h - 2.0), color);
}

} // namespace hmi::SceneGraphShapes
