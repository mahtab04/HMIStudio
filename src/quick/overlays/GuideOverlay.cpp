#include "quick/overlays/GuideOverlay.h"

#include "quick/overlays/SceneGraphShapes.h"

#include <QSGNode>

#include <cmath>

namespace hmi
{

GuideOverlay::GuideOverlay(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::NoButton);
}

void GuideOverlay::setGuides(std::vector<QLineF> viewLines)
{
    if (viewLines == m_lines)
        return;
    m_lines = std::move(viewLines);
    update();
}

void GuideOverlay::setColor(const QColor& color)
{
    if (color == m_color)
        return;
    m_color = color;
    update();
}

QSGNode* GuideOverlay::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    QSGNode* root = oldNode ? oldNode : new QSGNode();
    SceneGraphShapes::clearChildren(root);
    if (QQuickWindow* w = window())
    {
        for (const QLineF& line : m_lines)
        {
            if (qFuzzyCompare(line.x1(), line.x2()))
            {
                const double x = std::round(line.x1());
                SceneGraphShapes::addRect(root, w,
                                          QRectF(QPointF(x, std::min(line.y1(), line.y2())),
                                                 QSizeF(1.0, std::abs(line.dy()) + 1.0)),
                                          m_color);
            }
            else if (qFuzzyCompare(line.y1(), line.y2()))
            {
                const double y = std::round(line.y1());
                SceneGraphShapes::addRect(root, w,
                                          QRectF(QPointF(std::min(line.x1(), line.x2()), y),
                                                 QSizeF(std::abs(line.dx()) + 1.0, 1.0)),
                                          m_color);
            }
        }
    }
    SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
    return root;
}

} // namespace hmi
