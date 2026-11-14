#pragma once

#include <QPoint>
#include <QPointF>

namespace hmi
{

// Kind of input device a gesture came from. Thresholds and hit tolerances
// depend on it (InputSettings).
enum class PointerDevice
{
    Mouse,
    Touch,
    Pen,
};

// One recognized input gesture. Tools see only these, never raw mouse or
// touch events, so every tool works with every device (ARCHITECTURE.md
// section 7.2).
//
// View positions are in CanvasItem coordinates (pixels); scene positions are
// in screen coordinates and are filled in by CanvasItem before delivery.
struct PointerGesture
{
    enum class Kind
    {
        Tap,
        DoubleTap,
        LongPress,   // touch and pen: finger held still
        ContextMenu, // mouse: right click
        DragBegin,   // primary pointer moved past the drag threshold
        DragUpdate,
        DragEnd,
        DragCancel, // Escape, a second finger landing, or a cancelled touch
        PanBegin,   // middle button or Space + left drag
        PanUpdate,
        PanEnd,
        PinchBegin, // two fingers: zoom and pan together
        PinchUpdate,
        PinchEnd,
        Wheel,
    };

    Kind kind = Kind::Tap;
    PointerDevice device = PointerDevice::Mouse;

    QPointF viewPos;      // current position (pinch: centre between fingers)
    QPointF startViewPos; // where the gesture started
    QPointF viewDelta;    // movement since the previous gesture of this sequence
    QPointF scenePos;
    QPointF startScenePos;

    double scaleDelta = 1.0; // pinch: scale change since the previous update
    QPoint angleDelta;       // wheel
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    // The drag started after a long press (touch rubber band).
    bool afterLongPress = false;

    [[nodiscard]] constexpr bool isTouch() const noexcept { return device == PointerDevice::Touch; }
};

} // namespace hmi
