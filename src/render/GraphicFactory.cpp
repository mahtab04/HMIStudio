#include "render/GraphicFactory.h"

#include "core/graphics/AssetStore.h"
#include "render/RasterGraphic.h"
#include "render/SvgGraphic.h"

namespace hmi::GraphicFactory
{

bool looksLikeSvg(const QByteArray& data)
{
    // An XML prolog or comments may come first; the root must be <svg>.
    const QByteArray head = data.left(4096);
    return head.contains("<svg");
}

std::unique_ptr<IGraphicSource> create(const Asset& asset)
{
    if (asset.suffix == u"svg" || looksLikeSvg(asset.data))
    {
        auto svg = std::make_unique<SvgGraphic>(asset.data);
        return svg->isNull() ? nullptr : std::move(svg);
    }
    auto raster = std::make_unique<RasterGraphic>(asset.data);
    return raster->isNull() ? nullptr : std::move(raster);
}

} // namespace hmi::GraphicFactory
