#pragma once

#include <QColor>
#include <QRectF>

class QQuickWindow;
class QSGNode;
class QSGTransformNode;

namespace hmi::SceneGraphShapes
{

// Overlay drawing built only from rectangle and transform nodes, which every
// Qt Quick scene graph backend renders, including the software renderer
// used without a GPU (custom geometry nodes are ignored there).

// Removes and deletes all children of `node`.
void clearChildren(QSGNode* node);

// Call after adding or removing nodes under an item's paint node. The
// software renderer positions nodes added to an already rendered subtree
// with a stale transform; marking the item's transform node dirty makes it
// recompute the whole subtree. Harmless on the other backends.
void refreshTransform(QSGTransformNode* itemTransform);

// Appends a filled rectangle.
void addRect(QSGNode* parent, QQuickWindow* window, const QRectF& rect, const QColor& color);

// Appends a one-pixel outline drawn inside `rect`.
void addOutline(QSGNode* parent, QQuickWindow* window, const QRectF& rect, const QColor& color);

} // namespace hmi::SceneGraphShapes
