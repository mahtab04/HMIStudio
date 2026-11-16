#pragma once

#include "core/model/HmiObject.h"

namespace hmi
{

// Family base for process symbols (tank, pump, valve, motor, pipe, heat
// exchanger, conveyor, sensor), drawn with C++ painter code so their state
// (level, running, open) can animate smoothly. Most are pictures with a
// stretch mode (HmiObject::designTransform); tank and pipe lay themselves
// out in their rectangle.
class SymbolObject : public HmiObject
{
protected:
    using HmiObject::HmiObject;
};

} // namespace hmi
