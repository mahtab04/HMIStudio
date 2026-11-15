#pragma once

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QPolygonF>
#include <QSizeF>
#include <QString>

#include <concepts>
#include <type_traits>
#include <variant>

namespace hmi
{

// Reference to a tag by name. Resolved against the TagDatabase in phase 6.
struct TagRef
{
    QString tagName;
    friend bool operator==(const TagRef&, const TagRef&) = default;
};

// Value of an enumerated property. The allowed values and their keys are in
// the descriptor's constraint, so the value carries only the number.
struct EnumValue
{
    int value = 0;
    friend constexpr auto operator<=>(const EnumValue&, const EnumValue&) = default;
};

// A list of points (polyline, polygon, pipe). Objects store them relative
// to their bounds, from (0, 0) to (1, 1), so resizing scales the shape.
using PointList = QPolygonF;

using PropertyValue = std::variant<bool, int, double, QString, QColor, QPointF, QSizeF, QFont,
                                   TagRef, EnumValue, PointList>;

// Kind of a PropertyValue, in the same order as the variant alternatives.
enum class ValueKind
{
    Bool,
    Int,
    Double,
    String,
    Color,
    Point,
    Size,
    Font,
    Tag,
    Enum,
    Points,
};

namespace detail
{

template <typename T, typename Variant>
struct IsAlternative : std::false_type
{
};

template <typename T, typename... Ts>
struct IsAlternative<T, std::variant<Ts...>> : std::bool_constant<(std::same_as<T, Ts> || ...)>
{
};

} // namespace detail

// True for the types a property can hold.
template <typename T>
concept PropertyType = detail::IsAlternative<T, PropertyValue>::value;

static_assert(std::variant_size_v<PropertyValue> == 11, "ValueKind must list every alternative");

[[nodiscard]] constexpr ValueKind valueKind(const PropertyValue& value) noexcept
{
    return static_cast<ValueKind>(value.index());
}

// Stable identifier of a value kind, for logs and error messages. Not
// translated.
[[nodiscard]] const char* valueKindName(ValueKind kind) noexcept;

} // namespace hmi
