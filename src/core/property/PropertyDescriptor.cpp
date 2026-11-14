#include "core/property/PropertyDescriptor.h"

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

Result<void> checkRange(const PropertyDescriptor& descriptor, double number)
{
    if (!std::isfinite(number))
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Property '%1' must be a finite number")
                             .arg(QLatin1StringView(descriptor.key)));
    }
    const Constraint& c = descriptor.constraint;
    if ((c.minimum && number < *c.minimum) || (c.maximum && number > *c.maximum))
    {
        return makeError(ErrorCode::OutOfRange,
                         QStringLiteral("Property '%1' value %2 is outside [%3, %4]")
                             .arg(QLatin1StringView(descriptor.key))
                             .arg(number)
                             .arg(c.minimum.value_or(-HUGE_VAL))
                             .arg(c.maximum.value_or(HUGE_VAL)));
    }
    return {};
}

} // namespace

Result<PropertyValue> validate(const PropertyDescriptor& descriptor, const PropertyValue& value)
{
    const ValueKind expected = valueKind(descriptor.defaultValue);
    PropertyValue result = value;

    if (expected == ValueKind::Double && valueKind(value) == ValueKind::Int)
        result = static_cast<double>(std::get<int>(value));

    if (valueKind(result) != expected)
    {
        return makeError(ErrorCode::TypeMismatch,
                         QStringLiteral("Property '%1' expects %2, got %3")
                             .arg(QLatin1StringView(descriptor.key),
                                  QLatin1StringView(valueKindName(expected)),
                                  QLatin1StringView(valueKindName(valueKind(value)))));
    }

    if (const auto* number = std::get_if<double>(&result))
    {
        if (auto ok = checkRange(descriptor, *number); !ok)
            return tl::unexpected(ok.error());
    }
    else if (const auto* integer = std::get_if<int>(&result))
    {
        if (auto ok = checkRange(descriptor, *integer); !ok)
            return tl::unexpected(ok.error());
    }
    else if (const auto* color = std::get_if<QColor>(&result))
    {
        if (!color->isValid())
        {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Property '%1' needs a valid colour")
                                 .arg(QLatin1StringView(descriptor.key)));
        }
    }
    else if (const auto* enumValue = std::get_if<EnumValue>(&result))
    {
        const auto options = descriptor.constraint.enumOptions;
        const bool known = std::ranges::any_of(options, [&](const EnumOption& o)
                                               { return o.value == enumValue->value; });
        if (!known)
        {
            return makeError(ErrorCode::OutOfRange, QStringLiteral("Property '%1' has no option %2")
                                                        .arg(QLatin1StringView(descriptor.key))
                                                        .arg(enumValue->value));
        }
    }
    return result;
}

const PropertyDescriptor* findDescriptor(std::span<const PropertyDescriptor> table,
                                         PropertyId id) noexcept
{
    const auto it = std::ranges::find(table, id, &PropertyDescriptor::id);
    return it == table.end() ? nullptr : &*it;
}

} // namespace hmi
