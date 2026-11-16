#pragma once

#include <QByteArray>
#include <QSize>
#include <QStringList>

namespace hmi
{

// Checks an SVG file on import for content the SVG backend does not draw
// (filters, scripts, foreign objects, ...) so the user can choose to import
// it anyway or rasterize it (ARCHITECTURE.md section 14.3).
namespace SvgInspector
{

// Names of unsupported elements found in the document, each once, sorted.
[[nodiscard]] QStringList unsupportedElements(const QByteArray& svg);

// The document rendered to a PNG of `pixelSize` (its natural size when
// empty). Empty on failure.
[[nodiscard]] QByteArray rasterize(const QByteArray& svg, QSize pixelSize = {});

} // namespace SvgInspector

} // namespace hmi
