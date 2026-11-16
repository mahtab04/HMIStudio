#include "quick/overlays/GridItem.h"

#include "quick/overlays/SceneGraphShapes.h"

#include <QImage>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGNode>
#include <QSGTexture>

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
                       const QColor& color, const QColor& background)
{
    if (screenViewRect == m_screenViewRect && qFuzzyCompare(zoom, m_zoom) &&
        qFuzzyCompare(spacing, m_spacing) && color == m_color && background == m_background)
    {
        return;
    }
    m_background = background;
    m_screenViewRect = screenViewRect;
    m_zoom = zoom;
    m_spacing = spacing;
    m_color = color;
    update();
}

// A dot at every grid point, as engineering tools for operator panels draw
// it (TIA Portal, WinCC). Every row of dots is the same one-pixel-high strip,
// so one small texture serves all rows: an image node per row, a few hundred
// nodes at most, on every scene graph backend.
QSGNode* GridItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    QSGNode* root = oldNode ? oldNode : new QSGNode();
    SceneGraphShapes::clearChildren(root);

    // Visible part of the screen only.
    const QRectF area = m_screenViewRect.intersected(boundingRect());
    if (window() && !area.isEmpty() && m_background.alpha() > 0)
        SceneGraphShapes::addRect(root, window(), area, m_background);
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
    // Whole view pixels, so every dot is one sharp pixel.
    const double left = std::floor(area.left());
    const int stripWidth = int(std::ceil(area.right()) - left) + 1;
    QImage strip(stripWidth, 1, QImage::Format_ARGB32_Premultiplied);
    strip.fill(Qt::transparent);
    const QRgb dot = qPremultiply(m_color.rgba());
    for (double x = firstLine(m_screenViewRect.left(), area.left()); x <= area.right(); x += step)
    {
        const int column = int(std::round(x) - left);
        if (column >= 0 && column < stripWidth)
            strip.setPixel(column, 0, dot);
    }
    QSGTexture* texture = window()->createTextureFromImage(strip);
    bool owner = true;
    for (double y = firstLine(m_screenViewRect.top(), area.top()); y <= area.bottom(); y += step)
    {
        QSGImageNode* row = window()->createImageNode();
        row->setTexture(texture);
        row->setOwnsTexture(owner); // the first row deletes the shared texture
        owner = false;
        row->setFiltering(QSGTexture::Nearest);
        row->setRect(QRectF(left, std::round(y), stripWidth, 1.0));
        root->appendChildNode(row);
    }
    if (owner)
        delete texture; // no row was visible
    SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
    return root;
}

} // namespace hmi
