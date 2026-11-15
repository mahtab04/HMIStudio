#pragma once

#include "core/model/Ids.h"
#include "core/spatial/Snapper.h"
#include "editor/tools/ITool.h"
#include "quick/overlays/SelectionGeometry.h"

#include <QHash>
#include <QPointF>
#include <QRectF>

#include <optional>
#include <vector>

namespace hmi
{

class HmiObject;
class ToolContext;

// Selects, moves, resizes and rotates objects.
//
// Mouse: click selects, Shift+click adds, drag on an object moves, drag on a
// handle resizes, drag on the rotation handle rotates, drag on empty space
// draws a rubber band (left-to-right selects enclosed objects, right-to-left
// also touched ones), right-click asks for the context menu.
// Touch: tap selects, tap in multi-select mode or long press adds, drag on an
// object moves, drag on a handle resizes or rotates, drag on empty space
// pans, and long press on empty space followed by a drag draws a rubber band.
//
// Moves and resizes snap to the grid and to other objects' edges and centres
// while snapping is on (Alt suspends it); rotation snaps to 45° steps within
// a few degrees, and to 15° steps with Shift.
class SelectTool : public ITool
{
public:
    explicit SelectTool(ToolContext& context);

    void deactivate() override;
    void handleGesture(const PointerGesture& gesture) override;
    bool keyPress(int key, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] Qt::CursorShape cursor() const override { return Qt::ArrowCursor; }

private:
    // Rotation snaps to multiples of 45° when this close (degrees).
    static constexpr double kRotationSnap = 3.0;
    static constexpr double kRotationStep = 15.0;

    enum class Mode
    {
        None,
        Move,
        Resize,
        Rotate,
        RubberBand,
        Pan,
    };

    void tap(const PointerGesture& gesture);
    void longPress(const PointerGesture& gesture);
    void dragBegin(const PointerGesture& gesture);
    void dragUpdate(const PointerGesture& gesture);
    void dragEnd(const PointerGesture& gesture);
    void cancel();

    [[nodiscard]] HmiObject* objectAt(const PointerGesture& gesture) const;
    [[nodiscard]] bool additive(const PointerGesture& gesture) const;
    [[nodiscard]] QPointF moveDelta(const PointerGesture& gesture) const;
    [[nodiscard]] QRectF resizedGeometry(const PointerGesture& gesture) const;
    [[nodiscard]] double rotationFor(const PointerGesture& gesture) const;
    [[nodiscard]] bool snapping(const PointerGesture& gesture) const;
    void startSnapper();
    [[nodiscard]] QRectF rubberBand(const PointerGesture& gesture) const;

    ToolContext& m_context;
    Mode m_mode = Mode::None;
    QHash<ObjectId, QRectF> m_startGeometry; // objects being moved or resized
    QRectF m_moveBounds; // rotated bounds of everything being moved, at the start
    ObjectId m_resizeId; // also the object being rotated
    double m_resizeRotation = 0.0;
    SelectionHandle m_handle = SelectionHandle::BottomRight;
    std::optional<Snapper> m_snapper;
    mutable std::vector<QLineF> m_guides; // from the last snap, for the preview
};

} // namespace hmi
