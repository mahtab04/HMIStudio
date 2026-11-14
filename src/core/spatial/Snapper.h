#pragma once

#include "core/model/Ids.h"

#include <QLineF>
#include <QPointF>
#include <QRectF>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace hmi
{

class Screen;

// Which part of a rectangle is being snapped on one axis: its left or top
// edge, its centre, or its right or bottom edge.
enum class SnapAnchor
{
    Start,
    Centre,
    End,
};

// A target coordinate on one axis. `source` is the rectangle that produced
// it (another object or the screen), for drawing a guide; grid targets have
// none.
struct SnapMatch
{
    double target = 0.0;
    double distance = 0.0;
    std::optional<QRectF> source;
};

// One way of finding snap targets (Strategy pattern): the grid, object edges
// and centres, later user guides.
class SnapStrategy
{
public:
    SnapStrategy() = default;
    virtual ~SnapStrategy() = default;
    SnapStrategy(const SnapStrategy&) = delete;
    SnapStrategy& operator=(const SnapStrategy&) = delete;
    SnapStrategy(SnapStrategy&&) = delete;
    SnapStrategy& operator=(SnapStrategy&&) = delete;

    // Nearest target for `value` on `axis` within `threshold`, if any.
    [[nodiscard]] virtual std::optional<SnapMatch>
    nearest(Qt::Orientation axis, double value, SnapAnchor anchor, double threshold) const = 0;
};

// Snaps the left/top or right/bottom edge to grid lines.
class GridSnapStrategy final : public SnapStrategy
{
public:
    explicit GridSnapStrategy(double spacing);
    [[nodiscard]] std::optional<SnapMatch>
    nearest(Qt::Orientation axis, double value, SnapAnchor anchor, double threshold) const override;

private:
    double m_spacing;
};

// Snaps edges and centres to the edges and centres of other objects and of
// the screen. Targets are collected once when a gesture starts and kept
// sorted, so each query is a binary search even with thousands of objects.
class ObjectSnapStrategy final : public SnapStrategy
{
public:
    // `exclude` are the objects being moved or resized.
    ObjectSnapStrategy(const Screen& screen, std::span<const ObjectId> exclude);
    [[nodiscard]] std::optional<SnapMatch>
    nearest(Qt::Orientation axis, double value, SnapAnchor anchor, double threshold) const override;

private:
    struct Target
    {
        double value;
        QRectF source;
    };
    std::vector<Target> m_x;
    std::vector<Target> m_y;
};

// Adjustment found by the Snapper: add `offset` to the moving rectangle (or
// to its moving edges) and draw `guides`, which are in scene coordinates.
struct SnapResult
{
    QPointF offset;
    std::vector<QLineF> guides;
};

// Applies snap strategies to a rectangle being moved or resized. On each
// axis the closest target of any strategy wins; object targets win ties, so
// alignment with a neighbour beats the grid. Thresholds are in scene units.
class Snapper
{
public:
    struct Settings
    {
        bool toGrid = true;
        bool toObjects = true;
        double gridSpacing = 8.0;
    };

    // Collects targets from `screen`, ignoring the objects in `exclude`.
    Snapper(const Screen& screen, std::span<const ObjectId> exclude, const Settings& settings);
    ~Snapper();
    Snapper(const Snapper&) = delete;
    Snapper& operator=(const Snapper&) = delete;
    Snapper(Snapper&&) noexcept;
    Snapper& operator=(Snapper&&) noexcept;

    // Snaps any edge or the centre of a rectangle being moved.
    [[nodiscard]] SnapResult snapMove(const QRectF& rect, double threshold) const;

    // Snaps the moving edges of a rectangle being resized; the offset is to
    // be added to those edges.
    struct Edges
    {
        bool left = false;
        bool top = false;
        bool right = false;
        bool bottom = false;
    };
    [[nodiscard]] SnapResult snapResize(const QRectF& rect, Edges moving, double threshold) const;

private:
    struct AxisMatch
    {
        SnapMatch match;
        double value = 0.0;
    };
    [[nodiscard]] std::optional<AxisMatch>
    best(Qt::Orientation axis, std::span<const std::pair<double, SnapAnchor>> values,
         double threshold) const;
    [[nodiscard]] static std::vector<QLineF> guides(const QRectF& rect,
                                                    const std::optional<AxisMatch>& x,
                                                    const std::optional<AxisMatch>& y);

    std::vector<std::unique_ptr<SnapStrategy>> m_strategies;
};

} // namespace hmi
