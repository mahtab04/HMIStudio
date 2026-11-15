#pragma once

#include "core/model/HmiObject.h"

namespace hmi
{

// Family base for operating elements (I/O field, button, switch, bar,
// slider, gauge, clock, touch area, ...): objects that show or set a value
// and, from phase 6, are bound to tags. Adds no properties.
class ElementObject : public HmiObject
{
protected:
    using HmiObject::HmiObject;
};

} // namespace hmi
