#pragma once

#include "core/graphics/IGraphicProvider.h"
#include "render/GraphicCache.h"

#include <map>
#include <memory>
#include <mutex>

namespace hmi
{

class AssetStore;
class IGraphicSource;

// Draws the project's assets for objects (IGraphicProvider): decodes each
// asset once (GraphicFactory), renders it at the pixel size it is shown at
// and keeps the result in a GraphicCache, so panning and repainting do not
// render SVG again. Thread-safe; owned by the application next to the
// project whose AssetStore it reads.
class GraphicLibrary final : public IGraphicProvider
{
public:
    explicit GraphicLibrary(const AssetStore& assets);
    ~GraphicLibrary() override;

    bool draw(QPainter& painter, const QString& assetId, const QRectF& target, GraphicFit fit,
              const GraphicStyle& style) const override;

    // The rectangle a graphic of `natural` size takes in `target`.
    [[nodiscard]] static QRectF fitted(const QRectF& target, QSizeF natural, GraphicFit fit);

    [[nodiscard]] QSizeF naturalSize(const QString& assetId) const;
    [[nodiscard]] const GraphicCache& cache() const noexcept { return m_cache; }

private:
    [[nodiscard]] std::shared_ptr<const IGraphicSource> source(const QString& assetId) const;

    const AssetStore& m_assets;
    mutable std::mutex m_mutex;
    mutable std::map<QString, std::shared_ptr<const IGraphicSource>> m_sources;
    mutable GraphicCache m_cache;
};

} // namespace hmi
