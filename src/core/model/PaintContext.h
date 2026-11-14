#pragma once

namespace hmi
{

// Information an object needs while painting that is not part of the object:
// the current zoom, for level of detail (ARCHITECTURE.md section 14.5).
struct PaintContext
{
    // Below this zoom, objects skip small text and fine detail.
    static constexpr double kLowDetailZoom = 0.35;

    double zoom = 1.0;

    [[nodiscard]] constexpr bool lowDetail() const noexcept { return zoom < kLowDetailZoom; }
};

} // namespace hmi
