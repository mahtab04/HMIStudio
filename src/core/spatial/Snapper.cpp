#include "core/spatial/Snapper.h"

#include "core/model/HmiObject.h"
#include "core/model/Screen.h"

#include <QSet>

#include <algorithm>
#include <cmath>

namespace hmi
{

GridSnapStrategy::GridSnapStrategy(double spacing)
    : m_spacing(spacing)
{
    Q_ASSERT(spacing > 0.0);
}

std::optional<SnapMatch> GridSnapStrategy::nearest(Qt::Orientation /*axis*/, double value,
                                                   SnapAnchor anchor, double threshold) const
{
    // Centres are not snapped to the grid: an odd-sized object would never
    // have both its edges and its centre on grid lines.
    if (anchor == SnapAnchor::Centre)
        return std::nullopt;
    const double target = std::round(value / m_spacing) * m_spacing;
    const double distance = std::abs(target - value);
    if (distance > threshold)
        return std::nullopt;
    return SnapMatch{.target = target, .distance = distance, .source = std::nullopt};
}

ObjectSnapStrategy::ObjectSnapStrategy(const Screen& screen, std::span<const ObjectId> exclude)
{
    QSet<ObjectId> skipped;
    for (const ObjectId& id : exclude)
        skipped.insert(id);

    const auto addRect = [this](const QRectF& r)
    {
        for (const double x : {r.left(), r.center().x(), r.right()})
            m_x.push_back({x, r});
        for (const double y : {r.top(), r.center().y(), r.bottom()})
            m_y.push_back({y, r});
    };
    addRect(QRectF(QPointF(0, 0), screen.size()));
    for (const auto& object : screen.objects())
    {
        if (!skipped.contains(object->id()))
            addRect(object->sceneBoundingRect());
    }
    const auto byValue = [](const Target& a, const Target& b)
    {
        return a.value < b.value;
    };
    std::ranges::sort(m_x, byValue);
    std::ranges::sort(m_y, byValue);
}

std::optional<SnapMatch> ObjectSnapStrategy::nearest(Qt::Orientation axis, double value,
                                                     SnapAnchor /*anchor*/, double threshold) const
{
    const std::vector<Target>& targets = axis == Qt::Horizontal ? m_x : m_y;
    auto it = std::ranges::lower_bound(targets, value - threshold, std::less<>{}, &Target::value);
    std::optional<SnapMatch> best;
    for (; it != targets.end() && it->value <= value + threshold; ++it)
    {
        const double distance = std::abs(it->value - value);
        if (!best || distance < best->distance)
            best = SnapMatch{.target = it->value, .distance = distance, .source = it->source};
        else if (it->value == best->target && best->source)
            best->source = best->source->united(it->source); // guide spans every match
    }
    return best;
}

Snapper::Snapper(const Screen& screen, std::span<const ObjectId> exclude, const Settings& settings)
{
    if (settings.toObjects)
        m_strategies.push_back(std::make_unique<ObjectSnapStrategy>(screen, exclude));
    if (settings.toGrid && settings.gridSpacing > 0.0)
        m_strategies.push_back(std::make_unique<GridSnapStrategy>(settings.gridSpacing));
}

Snapper::~Snapper() = default;
Snapper::Snapper(Snapper&&) noexcept = default;
Snapper& Snapper::operator=(Snapper&&) noexcept = default;

std::optional<Snapper::AxisMatch>
Snapper::best(Qt::Orientation axis, std::span<const std::pair<double, SnapAnchor>> values,
              double threshold) const
{
    std::optional<AxisMatch> best;
    // Strategies are in priority order, so a later one must be strictly
    // closer to win.
    for (const auto& strategy : m_strategies)
    {
        for (const auto& [value, anchor] : values)
        {
            const auto match = strategy->nearest(axis, value, anchor, threshold);
            if (match && (!best || match->distance < best->match.distance))
                best = AxisMatch{.match = *match, .value = value};
        }
    }
    return best;
}

std::vector<QLineF> Snapper::guides(const QRectF& rect, const std::optional<AxisMatch>& x,
                                    const std::optional<AxisMatch>& y)
{
    std::vector<QLineF> lines;
    if (x && x->match.source)
    {
        const QRectF& s = *x->match.source;
        const double t = x->match.target;
        lines.emplace_back(t, std::min(rect.top(), s.top()), t,
                           std::max(rect.bottom(), s.bottom()));
    }
    if (y && y->match.source)
    {
        const QRectF& s = *y->match.source;
        const double t = y->match.target;
        lines.emplace_back(std::min(rect.left(), s.left()), t, std::max(rect.right(), s.right()),
                           t);
    }
    return lines;
}

SnapResult Snapper::snapMove(const QRectF& rect, double threshold) const
{
    const std::pair<double, SnapAnchor> xs[] = {{rect.left(), SnapAnchor::Start},
                                                {rect.center().x(), SnapAnchor::Centre},
                                                {rect.right(), SnapAnchor::End}};
    const std::pair<double, SnapAnchor> ys[] = {{rect.top(), SnapAnchor::Start},
                                                {rect.center().y(), SnapAnchor::Centre},
                                                {rect.bottom(), SnapAnchor::End}};
    const auto x = best(Qt::Horizontal, xs, threshold);
    const auto y = best(Qt::Vertical, ys, threshold);
    SnapResult result;
    result.offset =
        QPointF(x ? x->match.target - x->value : 0.0, y ? y->match.target - y->value : 0.0);
    result.guides = guides(rect.translated(result.offset), x, y);
    return result;
}

SnapResult Snapper::snapResize(const QRectF& rect, Edges moving, double threshold) const
{
    std::vector<std::pair<double, SnapAnchor>> xs;
    std::vector<std::pair<double, SnapAnchor>> ys;
    if (moving.left)
        xs.emplace_back(rect.left(), SnapAnchor::Start);
    if (moving.right)
        xs.emplace_back(rect.right(), SnapAnchor::End);
    if (moving.top)
        ys.emplace_back(rect.top(), SnapAnchor::Start);
    if (moving.bottom)
        ys.emplace_back(rect.bottom(), SnapAnchor::End);
    const auto x = best(Qt::Horizontal, xs, threshold);
    const auto y = best(Qt::Vertical, ys, threshold);

    SnapResult result;
    result.offset =
        QPointF(x ? x->match.target - x->value : 0.0, y ? y->match.target - y->value : 0.0);
    QRectF snapped = rect;
    if (moving.left)
        snapped.setLeft(snapped.left() + result.offset.x());
    if (moving.right)
        snapped.setRight(snapped.right() + result.offset.x());
    if (moving.top)
        snapped.setTop(snapped.top() + result.offset.y());
    if (moving.bottom)
        snapped.setBottom(snapped.bottom() + result.offset.y());
    result.guides = guides(snapped, x, y);
    return result;
}

} // namespace hmi
