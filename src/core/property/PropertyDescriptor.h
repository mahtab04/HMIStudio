#pragma once

#include "core/Result.h"
#include "core/model/Ids.h"
#include "core/property/PropertyValue.h"

#include <optional>
#include <span>

namespace hmi
{

// What a styled property stands for, so one project style can set it on
// every type that has it. Only properties whose meaning matches carry a
// role: a gauge's red needle is not an "accent", and a tank's liquid is not
// a "face".
enum class StyleRole
{
    None,
    Face,   // background of fields and dials (I/O field, list box, clock)
    Line,   // outline of operating elements
    Text,   // text colour
    Accent, // marks, tracks and bar fills (check box, slider, bar)
    Font,   // font family (sizes and weights stay per object)
};

// One allowed value of an enumerated property. `key` is the identifier saved
// in files; `displayName` is translated in the "hmi::Property" context.
struct EnumOption
{
    int value = 0;
    const char* key = nullptr;
    const char* displayName = nullptr;
};

// Limits a value must satisfy. For a point list, minimum and maximum bound
// the number of points. `readOnly` is advice for editors (the property
// panel shows the value without an input); the model itself may still set it.
struct Constraint
{
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::span<const EnumOption> enumOptions;
    bool readOnly = false;
};

// Describes one property of an object type. Object types publish a table of
// these, and the property panel, serializer and tag bindings all work from it
// instead of from the concrete class.
//
// `key` is the identifier written to files and must hash to `id`. Display
// name and category are translated in the "hmi::Property" context.
struct PropertyDescriptor
{
    PropertyId id;
    const char* key = nullptr;
    const char* displayName = nullptr;
    const char* category = nullptr;
    PropertyValue defaultValue;
    Constraint constraint{};
    bool bindable = false;
    // Set by the project style (HmiStyle) when not None.
    StyleRole styleRole = StyleRole::None;
    // The object's process value (a level, a running state): the property
    // the properties panel offers a tag for first. Bindable.
    bool process = false;
};

// Checks `value` against the descriptor's type and constraint. Returns the
// value to store: an int given for a double property is widened; everything
// else is returned unchanged or rejected.
[[nodiscard]] Result<PropertyValue> validate(const PropertyDescriptor& descriptor,
                                             const PropertyValue& value);

// Finds the descriptor for `id` in a table, or nullptr.
[[nodiscard]] const PropertyDescriptor* findDescriptor(std::span<const PropertyDescriptor> table,
                                                       PropertyId id) noexcept;

} // namespace hmi
