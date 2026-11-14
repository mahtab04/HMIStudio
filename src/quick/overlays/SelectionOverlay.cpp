#include "quick/overlays/SelectionOverlay.h"

#include "quick/overlays/SceneGraphShapes.h"

#include <QMatrix4x4>
#include <QSGTransformNode>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

QRectF handleRect(QPointF centre)
{
    const double half = SelectionOverlay::kHandleSize / 2.0;
    return {std::round(centre.x()) - half, std::round(centre.y()) - half,
            SelectionOverlay::kHandleSize, SelectionOverlay::kHandleSize};
}

} // namespace

SelectionOverlay::SelectionOverlay(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::NoButton);
}

void SelectionOverlay::setShapes(std::vector<Frame> frames, std::vector<QPointF> handles,
                                 std::optional<QRectF> rubberBand)
{
    m_frames = std::move(frames);
    m_handles = std::move(handles);
    m_rubberBand = rubberBand;
    update();
}

void SelectionOverlay::setColors(const QColor& accent, const QColor& handleFill,
                                 const QColor& rubberFill)
{
    m_accent = accent;
    m_handleFill = handleFill;
    m_rubberFill = rubberFill;
    update();
}

// The few nodes are rebuilt on each change; draw order is rubber band,
// frames, then handles on top.
QSGNode* SelectionOverlay::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    QSGNode* root = oldNode ? oldNode : new QSGNode();
    SceneGraphShapes::clearChildren(root);
    QQuickWindow* w = window();
    if (!w)
    {
        SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
        return root;
    }

    if (m_rubberBand)
    {
        const QRectF band = m_rubberBand->normalized();
        SceneGraphShapes::addRect(root, w, band, m_rubberFill);
        SceneGraphShapes::addOutline(root, w, band, m_accent);
    }

    for (const Frame& frame : m_frames)
    {
        if (qFuzzyIsNull(std::fmod(frame.rotation, 360.0)))
        {
            const QSizeF size(std::max(frame.size.width(), 1.0),
                              std::max(frame.size.height(), 1.0));
            SceneGraphShapes::addOutline(
                root, w,
                QRectF(QPointF(std::round(frame.centre.x() - size.width() / 2.0),
                               std::round(frame.centre.y() - size.height() / 2.0)),
                       size),
                m_accent);
            continue;
        }
        auto* transform = new QSGTransformNode();
        QMatrix4x4 matrix;
        matrix.translate(float(frame.centre.x()), float(frame.centre.y()));
        matrix.rotate(float(frame.rotation), 0.0f, 0.0f, 1.0f);
        transform->setMatrix(matrix);
        const QSizeF size(std::max(frame.size.width(), 1.0), std::max(frame.size.height(), 1.0));
        SceneGraphShapes::addOutline(
            transform, w, QRectF(QPointF(-size.width() / 2.0, -size.height() / 2.0), size),
            m_accent);
        root->appendChildNode(transform);
    }

    for (const QPointF& h : m_handles)
    {
        const QRectF r = handleRect(h);
        SceneGraphShapes::addRect(root, w, r, m_accent);
        SceneGraphShapes::addRect(root, w, r.adjusted(1, 1, -1, -1), m_handleFill);
    }
    SceneGraphShapes::refreshTransform(data ? data->transformNode : nullptr);
    return root;
}

} // namespace hmi
