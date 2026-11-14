#pragma once

#include "quick/input/PointerGesture.h"

#include <Qt>

namespace hmi
{

// A canvas tool (State pattern): ToolController forwards gestures and keys to
// the current one. A tool previews on the canvas while a gesture runs and
// changes the model only when it ends, with one command. Cancelling leaves
// the model untouched.
class ITool
{
public:
    ITool() = default;
    virtual ~ITool() = default;
    ITool(const ITool&) = delete;
    ITool& operator=(const ITool&) = delete;
    ITool(ITool&&) = delete;
    ITool& operator=(ITool&&) = delete;

    virtual void activate() {}
    // Must drop any preview in progress.
    virtual void deactivate() {}
    virtual void handleGesture(const PointerGesture& gesture) = 0;
    // Returns true when the key was used.
    virtual bool keyPress(int /*key*/, Qt::KeyboardModifiers /*modifiers*/) { return false; }
    [[nodiscard]] virtual Qt::CursorShape cursor() const = 0;
};

} // namespace hmi
