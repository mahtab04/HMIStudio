#pragma once

#include "core/Result.h"

#include <QColor>
#include <QString>

#include <optional>
#include <vector>

namespace hmi
{

// How a graphic fills its object's rectangle.
enum class GraphicFit
{
    Stretch, // fill the rectangle, ignoring the aspect ratio
    Fit,     // as large as fits, keeping the aspect ratio
    Fill,    // cover the rectangle, keeping the aspect ratio, cropped
    Centre,  // natural size, centred
};

// Changes to named parts of an SVG graphic (ARCHITECTURE.md section 14.4):
// a fill or stroke colour, or hiding the element with that id. Written as
// text in the graphic view's "styleOverrides" property:
//
//     body: fill #3B6FD4; ring: hidden; outline: stroke #000000
//
// Bindings will write into it in phase 6, so a tag can drive any named part.
struct GraphicStyle
{
    struct Override
    {
        QString elementId;
        std::optional<QColor> fill;
        std::optional<QColor> stroke;
        bool hidden = false;
        friend bool operator==(const Override&, const Override&) = default;
    };

    std::vector<Override> overrides;

    [[nodiscard]] bool isEmpty() const noexcept { return overrides.empty(); }
    [[nodiscard]] const Override* find(QStringView elementId) const;
    // Stable hash for cache keys; 0 for no overrides.
    [[nodiscard]] std::size_t hash() const;

    // Parses the text form; an empty text gives an empty style.
    [[nodiscard]] static Result<GraphicStyle> parse(const QString& text);
    [[nodiscard]] QString toString() const;

    friend bool operator==(const GraphicStyle&, const GraphicStyle&) = default;
};

} // namespace hmi
