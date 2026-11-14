#pragma once

#include "core/property/PropertyDescriptor.h"

#include <QSizeF>

#include <initializer_list>
#include <vector>

namespace hmi
{

// Translation context and category names for descriptor tables.
namespace PropertyCategory
{
inline constexpr const char* General = "General";
inline constexpr const char* Layout = "Layout";
inline constexpr const char* Appearance = "Appearance";
inline constexpr const char* Text = "Text";
inline constexpr const char* Behaviour = "Behaviour";
} // namespace PropertyCategory

// Builds an object type's descriptor table: the properties every object has
// (name, geometry, rotation, visibility, lock) followed by `specific`.
// `defaultSize` sets the width and height defaults for this type. Asserts in
// debug builds that no two ids collide and every key hashes to its id.
[[nodiscard]] std::vector<PropertyDescriptor>
withCommonDescriptors(QSizeF defaultSize, std::initializer_list<PropertyDescriptor> specific);

} // namespace hmi
