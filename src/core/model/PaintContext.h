#pragma once

namespace hmi
{

class IGraphicProvider;

// Information an object needs while painting that is not part of the object:
// the current zoom, for level of detail (ARCHITECTURE.md section 14.5);
// whether it is drawn in the editor; and where project graphics come from.
struct PaintContext
{
    // Below this zoom, objects skip small text and fine detail.
    static constexpr double kLowDetailZoom = 0.35;

    double zoom = 1.0;
    // In the editor, objects that are invisible at runtime (touch areas,
    // empty graphic views) draw a marked outline so they can be found.
    bool editing = true;
    // Draws imported images and SVG; null where none are available.
    const IGraphicProvider* graphics = nullptr;

    [[nodiscard]] constexpr bool lowDetail() const noexcept { return zoom < kLowDetailZoom; }
};

} // namespace hmi
