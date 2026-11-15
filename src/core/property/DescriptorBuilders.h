#pragma once

#include "core/property/PropertyDescriptor.h"

#include <QColor>

#include <optional>
#include <span>

// Short ways to write the descriptor entries that many object types share,
// so a type's table reads as a list of its properties. Display names are
// passed through QT_TRANSLATE_NOOP("hmi::Property", ...) at the call site so
// lupdate finds them; each key must hash to its id (checked by
// withCommonDescriptors).
namespace hmi::Describe
{

[[nodiscard]] PropertyDescriptor fillColor(const QColor& value, const char* displayName = nullptr);
[[nodiscard]] PropertyDescriptor lineColor(const QColor& value);
[[nodiscard]] PropertyDescriptor lineWidth(double value);
[[nodiscard]] PropertyDescriptor text(const QString& value);
[[nodiscard]] PropertyDescriptor textColor(const QColor& value);
[[nodiscard]] PropertyDescriptor font(int pixelSize, bool bold = false);

// Value, minimum and maximum of a value display (bar, gauge, slider, ...).
[[nodiscard]] PropertyDescriptor value(double value);
[[nodiscard]] PropertyDescriptor minimum(double value);
[[nodiscard]] PropertyDescriptor maximum(double value);

[[nodiscard]] PropertyDescriptor number(PropertyId id, const char* key, const char* displayName,
                                        const char* category, double value,
                                        std::optional<double> minimum = std::nullopt,
                                        std::optional<double> maximum = std::nullopt,
                                        bool bindable = false);
[[nodiscard]] PropertyDescriptor integer(PropertyId id, const char* key, const char* displayName,
                                         const char* category, int value,
                                         std::optional<double> minimum = std::nullopt,
                                         std::optional<double> maximum = std::nullopt,
                                         bool bindable = false);
[[nodiscard]] PropertyDescriptor flag(PropertyId id, const char* key, const char* displayName,
                                      const char* category, bool value, bool bindable = false);
[[nodiscard]] PropertyDescriptor string(PropertyId id, const char* key, const char* displayName,
                                        const char* category, const QString& value,
                                        bool bindable = false);
[[nodiscard]] PropertyDescriptor color(PropertyId id, const char* key, const char* displayName,
                                       const char* category, const QColor& value,
                                       bool bindable = false);
[[nodiscard]] PropertyDescriptor choice(PropertyId id, const char* key, const char* displayName,
                                        const char* category, int value,
                                        std::span<const EnumOption> options, bool bindable = false);
[[nodiscard]] PropertyDescriptor points(const PointList& value, int minimumCount);

} // namespace hmi::Describe
