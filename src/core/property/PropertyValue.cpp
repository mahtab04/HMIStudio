#include "core/property/PropertyValue.h"

namespace hmi
{

const char* valueKindName(ValueKind kind) noexcept
{
    switch (kind)
    {
    case ValueKind::Bool:
        return "bool";
    case ValueKind::Int:
        return "int";
    case ValueKind::Double:
        return "double";
    case ValueKind::String:
        return "string";
    case ValueKind::Color:
        return "color";
    case ValueKind::Point:
        return "point";
    case ValueKind::Size:
        return "size";
    case ValueKind::Font:
        return "font";
    case ValueKind::Tag:
        return "tag";
    case ValueKind::Enum:
        return "enum";
    }
    return "unknown";
}

} // namespace hmi
