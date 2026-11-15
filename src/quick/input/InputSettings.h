#pragma once

#include "quick/input/PointerGesture.h"

namespace hmi
{

// Thresholds for gesture recognition and hit testing, per device. Touch gets
// larger values because a fingertip is imprecise and moves while tapping.
// Distances are in view pixels.
struct InputSettings
{
    double mouseDragDistance = 4.0;
    double touchDragDistance = 12.0;
    double penDragDistance = 6.0;

    double mouseHitTolerance = 3.0;
    double touchHitTolerance = 12.0;
    double penHitTolerance = 5.0;

    // Side of the square around a resize handle that counts as hitting it.
    // 44 px for touch, the minimum touch target size.
    double mouseHandleHitSize = 12.0;
    double touchHandleHitSize = 44.0;
    double penHandleHitSize = 20.0;

    // How close an edge or centre must come to a snap target to jump to it.
    double mouseSnapDistance = 6.0;
    double touchSnapDistance = 12.0;
    double penSnapDistance = 8.0;

    // Distance of the rotation handle above the selection frame.
    double rotationHandleOffset = 28.0;

    int longPressMs = 500;
    int doubleTapMs = 400;
    double doubleTapDistance = 24.0;

    [[nodiscard]] constexpr double dragDistance(PointerDevice device) const noexcept
    {
        return pick(device, mouseDragDistance, touchDragDistance, penDragDistance);
    }
    [[nodiscard]] constexpr double hitTolerance(PointerDevice device) const noexcept
    {
        return pick(device, mouseHitTolerance, touchHitTolerance, penHitTolerance);
    }
    [[nodiscard]] constexpr double handleHitSize(PointerDevice device) const noexcept
    {
        return pick(device, mouseHandleHitSize, touchHandleHitSize, penHandleHitSize);
    }

    [[nodiscard]] constexpr double snapDistance(PointerDevice device) const noexcept
    {
        return pick(device, mouseSnapDistance, touchSnapDistance, penSnapDistance);
    }

private:
    [[nodiscard]] static constexpr double pick(PointerDevice device, double mouse, double touch,
                                               double pen) noexcept
    {
        switch (device)
        {
        case PointerDevice::Touch:
            return touch;
        case PointerDevice::Pen:
            return pen;
        case PointerDevice::Mouse:
            break;
        }
        return mouse;
    }
};

} // namespace hmi
