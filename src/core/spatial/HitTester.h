#pragma once

#include <QPointF>
#include <QRectF>

#include <vector>

namespace hmi
{

class HmiObject;
class Screen;

// How hit testing treats objects. `tolerance` is in scene units: the caller
// converts its device tolerance (larger for touch) by dividing by the zoom.
struct HitOptions
{
    double tolerance = 0.0;
    bool includeLocked = true;
    bool includeHidden = false;
};

// Finds objects under a point or inside a rectangle by testing each object's
// hitShape(), from the top of the z-order down. A linear walk; a spatial
// index is added only if profiling at 2,000 objects shows a need
// (ARCHITECTURE.md section 7.2).
class HitTester
{
public:
    enum class RectMode
    {
        Contains,   // object's bounding rectangle lies fully inside
        Intersects, // object's hit shape touches the rectangle
    };

    // True when `scenePos` is on the object or within `tolerance` of it.
    [[nodiscard]] static bool hits(const HmiObject& object, QPointF scenePos, double tolerance);

    [[nodiscard]] static HmiObject* topmostAt(const Screen& screen, QPointF scenePos,
                                              const HitOptions& options = {});

    // Every object under the point, topmost first.
    [[nodiscard]] static std::vector<HmiObject*> allAt(const Screen& screen, QPointF scenePos,
                                                       const HitOptions& options = {});

    // Objects selected by a rubber band, in z-order from bottom to top.
    [[nodiscard]] static std::vector<HmiObject*> inRect(const Screen& screen,
                                                        const QRectF& sceneRect, RectMode mode,
                                                        const HitOptions& options = {});
};

} // namespace hmi
