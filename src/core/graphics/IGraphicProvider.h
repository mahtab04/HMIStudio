#pragma once

#include "core/graphics/GraphicStyle.h"

#include <QRectF>
#include <QString>

class QPainter;

namespace hmi
{

// Draws project assets (images, SVG) for objects. Objects reach it through
// PaintContext::graphics, so hmi_core does not depend on the rendering code
// (hmi_render implements it as GraphicLibrary). Called on the render
// thread: implementations must be thread-safe.
class IGraphicProvider
{
public:
    IGraphicProvider() = default;
    virtual ~IGraphicProvider() = default;
    IGraphicProvider(const IGraphicProvider&) = delete;
    IGraphicProvider& operator=(const IGraphicProvider&) = delete;
    IGraphicProvider(IGraphicProvider&&) = delete;
    IGraphicProvider& operator=(IGraphicProvider&&) = delete;

    // Draws the asset into `target` (local coordinates). Returns false when
    // the asset is unknown or cannot be decoded; the caller then draws a
    // placeholder.
    virtual bool draw(QPainter& painter, const QString& assetId, const QRectF& target,
                      GraphicFit fit, const GraphicStyle& style) const = 0;
};

} // namespace hmi
