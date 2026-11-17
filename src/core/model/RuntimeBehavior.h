#pragma once

#include "core/model/Ids.h"

namespace hmi
{

// Semantic input capabilities supplied by each object type. Runtime code
// operates these capabilities without depending on concrete object classes.
struct RuntimeBehavior
{
    enum class Kind
    {
        None,
        ValueInput,
        Slider,
        Toggle,
        Choice,
        RowChoice,
        Action
    };
    Kind kind = Kind::None;
    PropertyId property;
};

enum class TagAction
{
    None,
    Set,
    Toggle,
    Increment,
    Momentary
};

} // namespace hmi
