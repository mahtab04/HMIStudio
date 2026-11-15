#pragma once

#include "render/IGraphicSource.h"

#include <QByteArray>

#include <memory>

namespace hmi
{

struct Asset;

// Picks RasterGraphic or SvgGraphic from an asset's content (Factory).
namespace GraphicFactory
{

[[nodiscard]] bool looksLikeSvg(const QByteArray& data);

// The decoded source, or null if the data is neither SVG nor a readable
// image.
[[nodiscard]] std::unique_ptr<IGraphicSource> create(const Asset& asset);

} // namespace GraphicFactory

} // namespace hmi
