#pragma once

#include "core/model/Ids.h"
#include "editor/tools/ITool.h"
#include "quick/overlays/SelectionGeometry.h"

#include <QHash>
#include <QPointF>
#include <QRectF>

#include <vector>

namespace hmi
{

class HmiObject;
class ToolContext;

// Selects, moves and resizes objects.
//
// Mouse: click selects, Shift+click adds, drag on an object moves, drag on a
// handle resizes, drag on empty space draws a rubber band (left-to-right
// selects enclosed objects, right-to-left also touched ones).
// Touch: tap selects, tap in multi-select mode or long press adds, drag on an
// object moves, drag on a handle resizes, drag on empty space pans, and long
// press on empty space followed by a drag draws a rubber band.
class SelectTool : public ITool
{
public:
    explicit SelectTool(ToolContext& context);

    void deactivate() override;
    void handleGesture(const PointerGesture& gesture) override;
    bool keyPress(int key, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] Qt::CursorShape cursor() const override { return Qt::ArrowCursor; }

private:
    enum class Mode
    {
        None,
        Move,
        Resize,
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
    [[nodiscard]] QRectF rubberBand(const PointerGesture& gesture) const;

    ToolContext& m_context;
    Mode m_mode = Mode::None;
    QHash<ObjectId, QRectF> m_startGeometry; // objects being moved or resized
    ObjectId m_resizeId;
    double m_resizeRotation = 0.0;
    SelectionHandle m_handle = SelectionHandle::BottomRight;
};

} // namespace hmi
