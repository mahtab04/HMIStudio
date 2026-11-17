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
        // Operated only through its events (buttons, touch areas).
        Action
    };
    Kind kind = Kind::None;
    PropertyId property;
};

} // namespace hmi
