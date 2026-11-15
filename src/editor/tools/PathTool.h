#pragma once

#include "core/model/Ids.h"
#include "editor/tools/ITool.h"

#include <QPointF>

#include <vector>

namespace hmi
{

class ToolContext;

// Places a polyline, polygon or pipe point by point (types registered with
// Placement::Points or OrthogonalPoints; this is the plan's PipeTool).
//
// Tap adds a point; a drag shows the next segment and adds its end point
// when released. Double-tap or Enter finishes, Backspace removes the last
// point, Escape cancels. Orthogonal types route with right-angled segments
// (Alt allows any angle). Points snap to the grid while snapping is on. The
// object is created with one command when the path is finished; then the
// select tool returns.
class PathTool : public ITool
{
public:
    explicit PathTool(ToolContext& context);

    void setType(ObjectTypeId type);
    [[nodiscard]] ObjectTypeId type() const noexcept { return m_type; }
    [[nodiscard]] const std::vector<QPointF>& points() const noexcept { return m_points; }

    void deactivate() override;
    void handleGesture(const PointerGesture& gesture) override;
    bool keyPress(int key, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] Qt::CursorShape cursor() const override { return Qt::CrossCursor; }

private:
    // Where a point at `scenePos` lands: on the grid, and in line with the
    // previous point for orthogonal routing.
    [[nodiscard]] QPointF constrained(QPointF scenePos, const PointerGesture& gesture) const;
    void addPoint(QPointF point);
    void showPreview(const std::vector<QPointF>& extra = {});
    void finish();
    void cancel();
    [[nodiscard]] int minimumPoints() const;

    ToolContext& m_context;
    ObjectTypeId m_type;
    bool m_orthogonal = false;
    std::vector<QPointF> m_points;
};

} // namespace hmi
