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
                                 std::optional<RotationHandle> rotationHandle,
                                 std::optional<QRectF> rubberBand)
{
    m_frames = std::move(frames);
    m_handles = std::move(handles);
    m_rotationHandle = rotationHandle;
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

    if (m_rotationHandle)
    {
        // Stem from the frame to the handle, then the handle as a diamond so
        // it does not look like a resize handle.
        const QLineF stem(m_rotationHandle->anchor, m_rotationHandle->handle);
        auto* stemNode = new QSGTransformNode();
        QMatrix4x4 stemMatrix;
        stemMatrix.translate(float(stem.x1()), float(stem.y1()));
        stemMatrix.rotate(float(-stem.angle()), 0.0f, 0.0f, 1.0f);
        stemNode->setMatrix(stemMatrix);
        SceneGraphShapes::addRect(stemNode, w, QRectF(0.0, -0.5, stem.length(), 1.0), m_accent);
        root->appendChildNode(stemNode);

        auto* handleNode = new QSGTransformNode();
        QMatrix4x4 handleMatrix;
        handleMatrix.translate(float(std::round(stem.x2())), float(std::round(stem.y2())));
        handleMatrix.rotate(45.0f, 0.0f, 0.0f, 1.0f);
        handleNode->setMatrix(handleMatrix);
        const double half = kRotationHandleSize / 2.0;
        const QRectF r(-half, -half, kRotationHandleSize, kRotationHandleSize);
        SceneGraphShapes::addRect(handleNode, w, r, m_accent);
        SceneGraphShapes::addRect(handleNode, w, r.adjusted(1, 1, -1, -1), m_handleFill);
        root->appendChildNode(handleNode);
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
