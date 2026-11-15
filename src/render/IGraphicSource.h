#pragma once

#include "core/graphics/GraphicStyle.h"

#include <QRectF>
#include <QSizeF>

class QPainter;

namespace hmi
{

// A decoded asset that can draw itself at any size: a raster image or an
// SVG document (ARCHITECTURE.md section 14.2). Thread-safe.
class IGraphicSource
{
public:
    IGraphicSource() = default;
    virtual ~IGraphicSource() = default;
    IGraphicSource(const IGraphicSource&) = delete;
    IGraphicSource& operator=(const IGraphicSource&) = delete;
    IGraphicSource(IGraphicSource&&) = delete;
    IGraphicSource& operator=(IGraphicSource&&) = delete;

    [[nodiscard]] virtual QSizeF naturalSize() const = 0;
    // Draws into `target`. Raster graphics ignore the style.
    virtual void render(QPainter& painter, const QRectF& target,
                        const GraphicStyle& style) const = 0;
    // Whether the result depends on the style (for cache keys).
    [[nodiscard]] virtual bool usesStyle() const = 0;
};

} // namespace hmi
