#pragma once

#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include <array>
#include <optional>

namespace hmi
{

// The eight resize handles, clockwise from the top-left corner.
enum class SelectionHandle
{
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
};

inline constexpr std::size_t kHandleCount = 8;

// Geometry of a selection frame shared by the overlay that draws it and the
// tools that hit-test it, so both always agree. `geometry` and `rotation` are
// an object's (or its preview's) values; results are in scene coordinates.
namespace SelectionGeometry
{

[[nodiscard]] QPolygonF frame(const QRectF& geometry, double rotation);

[[nodiscard]] std::array<QPointF, kHandleCount> handles(const QRectF& geometry, double rotation);

// Handle under `scenePos`, if any. `hitSize` is the side of the square hit
// area in scene units.
[[nodiscard]] std::optional<SelectionHandle> handleAt(const QRectF& geometry, double rotation,
                                                      QPointF scenePos, double hitSize);

// New geometry after dragging `handle` by `sceneDelta`. The opposite edge or
// corner stays fixed in the scene, also for rotated objects. Sizes do not go
// below `minimumSize` (or zero when the start size is already zero).
[[nodiscard]] QRectF resized(const QRectF& geometry, double rotation, SelectionHandle handle,
                             QPointF sceneDelta, double minimumSize = 1.0);

} // namespace SelectionGeometry

} // namespace hmi
