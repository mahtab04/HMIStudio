#pragma once

#include "quick/ICanvasView.h"

#include <QHash>

#include <cmath>
#include <optional>

namespace hmi::test
{

// Records what tools ask of the canvas. View and scene coordinates coincide
// at zoom 1 with no pan, which keeps gesture construction simple.
class FakeCanvas : public ICanvasView
{
public:
    double zoomValue = 1.0;
    QPointF pan;
    QHash<ObjectId, QRectF> preview;
    QHash<ObjectId, double> previewRotation;
    std::vector<QLineF> guides;
    std::optional<QRectF> rubberBand;
    Qt::CursorShape cursor = Qt::ArrowCursor;
    int zoomCalls = 0;
    int clearPreviewCalls = 0;

    void reset()
    {
        zoomValue = 1.0;
        pan = {};
        preview.clear();
        previewRotation.clear();
        guides.clear();
        rubberBand.reset();
        cursor = Qt::ArrowCursor;
        zoomCalls = 0;
        clearPreviewCalls = 0;
    }

    [[nodiscard]] double zoom() const override { return zoomValue; }
    void zoomAt(QPointF /*viewPos*/, double factor) override
    {
        zoomValue *= factor;
        ++zoomCalls;
    }
    void panBy(QPointF viewDelta) override { pan += viewDelta; }
    void setPreviewGeometry(ObjectId id, const QRectF& sceneGeometry) override
    {
        preview.insert(id, sceneGeometry);
    }
    void setPreviewRotation(ObjectId id, double rotation) override
    {
        previewRotation.insert(id, rotation);
    }
    void setGuides(std::vector<QLineF> sceneLines) override { guides = std::move(sceneLines); }
    void clearPreview() override
    {
        preview.clear();
        previewRotation.clear();
        ++clearPreviewCalls;
    }
    void setRubberBand(const std::optional<QRectF>& sceneRect) override { rubberBand = sceneRect; }
    void setToolCursor(Qt::CursorShape shape) override { cursor = shape; }
};

} // namespace hmi::test
