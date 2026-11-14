#include "quick/overlays/GridItem.h"

#include "quick/overlays/SceneGraphShapes.h"

#include <QSGNode>

#include <cmath>

namespace hmi
{

GridItem::GridItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::NoButton);
}

void GridItem::setGrid(const QRectF& screenViewRect, double zoom, double spacing,
                       const QColor& color)
{
    if (screenViewRect == m_screenViewRect && qFuzzyCompare(zoom, m_zoom) &&
        qFuzzyCompare(spacing, m_spacing) && color == m_color)
    {
        return;
    }
    m_screenViewRect = screenViewRect;
    m_zoom = zoom;
    m_spacing = spacing;
    m_color = color;
    update();
}

// One-pixel rectangle nodes, so every scene graph backend draws the grid.
QSGNode* GridItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    QSGNode* root = oldNode ? oldNode : new QSGNode();
    SceneGraphShapes::clearChildren(root);

    // Visible part of the screen only.
    const QRectF area = m_screenViewRect.intersected(boundingRect());
    double step = m_spacing * m_zoom;
    if (!window() || step <= 0.0 || area.isEmpty() || m_color.alpha() == 0)
    {
        SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
        return root;
    }
    while (step < kMinimumViewSpacing)
        step *= 2.0;

    const auto firstLine = [&](double origin, double from)
    {
        return origin + std::ceil((from - origin) / step) * step;
    };
    for (double x = firstLine(m_screenViewRect.left(), area.left()); x <= area.right(); x += step)
    {
        SceneGraphShapes::addRect(root, window(),
                                  QRectF(std::round(x), area.top(), 1.0, area.height()), m_color);
    }
    for (double y = firstLine(m_screenViewRect.top(), area.top()); y <= area.bottom(); y += step)
    {
        SceneGraphShapes::addRect(root, window(),
                                  QRectF(area.left(), std::round(y), area.width(), 1.0), m_color);
    }
    SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
    return root;
}

} // namespace hmi
