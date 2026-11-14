#pragma once

#include "core/model/HmiObject.h"

namespace hmi
{

// Family base for basic drawing objects (line, rectangle, ellipse, text box,
// later polyline, polygon and graphic view). It adds no properties; it groups
// the family so later phases can give shapes shared behaviour.
class ShapeObject : public HmiObject
{
protected:
    using HmiObject::HmiObject;
};

} // namespace hmi
