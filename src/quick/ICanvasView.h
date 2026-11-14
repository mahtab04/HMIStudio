#pragma once

#include "core/model/Ids.h"

#include <QPointF>
#include <QRectF>

#include <optional>

namespace hmi
{

// What a tool may do to the canvas view: navigate and show previews. Tools
// change the document only through commands; everything here is transient
// view state. CanvasItem implements it; tests use a fake.
class ICanvasView
{
public:
    ICanvasView() = default;
    virtual ~ICanvasView() = default;
    ICanvasView(const ICanvasView&) = delete;
    ICanvasView& operator=(const ICanvasView&) = delete;
    ICanvasView(ICanvasView&&) = delete;
    ICanvasView& operator=(ICanvasView&&) = delete;

    // View pixels per scene unit.
    [[nodiscard]] virtual double zoom() const = 0;

    // Multiplies the zoom by `factor`, keeping `viewPos` fixed on screen.
    virtual void zoomAt(QPointF viewPos, double factor) = 0;
    virtual void panBy(QPointF viewDelta) = 0;

    // Shows an object at `sceneGeometry` instead of its model geometry until
    // clearPreview(). The model is not touched.
    virtual void setPreviewGeometry(ObjectId id, const QRectF& sceneGeometry) = 0;
    virtual void clearPreview() = 0;

    // Rubber band or placement rectangle in scene coordinates; nullopt hides it.
    virtual void setRubberBand(const std::optional<QRectF>& sceneRect) = 0;

    virtual void setToolCursor(Qt::CursorShape shape) = 0;
};

} // namespace hmi
