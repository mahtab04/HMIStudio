#pragma once

#include "core/model/HmiObject.h"

namespace hmi
{

// Family base for complex controls (alarm view, trend views, screen
// window, browser, media player, recipe view, diagnostics). In the editor
// they draw a placeholder of their layout; live behaviour comes in phase 7.
class ControlObject : public HmiObject
{
protected:
    using HmiObject::HmiObject;
};

} // namespace hmi
