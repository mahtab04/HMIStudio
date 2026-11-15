#pragma once

#include "core/Result.h"
#include "core/property/PropertyDescriptor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;

// Objects as JSON, driven by the property descriptors so a new object type
// needs no code here (ARCHITECTURE.md section 10):
//
//     { "type": "rectangle", "id": "…", "props": { "x": 10, … },
//       "children": [ … ] }
//
// Used by the clipboard now and by the project serializer from phase 4.
// Values are written by kind: colours as "#AARRGGBB", points and sizes as
// two-number arrays, fonts as QFont::toString(), enums by option key.
namespace ObjectJson
{

[[nodiscard]] QJsonObject write(const HmiObject& object);

// Creates an object of the registered type and applies every known
// property. Unknown properties and keys are kept in the object's
// preservedJson() and written back by write(); an unknown type becomes an
// UnknownObject that keeps the whole entry. A known property with a value
// of the wrong kind or outside its constraint is an error.
[[nodiscard]] Result<std::unique_ptr<HmiObject>> read(const QJsonObject& json);

[[nodiscard]] QJsonArray writeAll(std::span<const HmiObject* const> objects);
[[nodiscard]] Result<std::vector<std::unique_ptr<HmiObject>>> readAll(const QJsonArray& json);

[[nodiscard]] QJsonValue writeValue(const PropertyDescriptor& descriptor,
                                    const PropertyValue& value);
[[nodiscard]] Result<PropertyValue> readValue(const PropertyDescriptor& descriptor,
                                              const QJsonValue& json);

} // namespace ObjectJson

} // namespace hmi
