#pragma once

#include "core/Result.h"
#include "core/model/Ids.h"
#include "core/property/PropertyDescriptor.h"
#include "core/tags/Tag.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <map>
#include <memory>

namespace hmi
{

// Turns a tag value into a property value (ARCHITECTURE.md section 5.3).
// Immutable; created through TransformRegistry from a kind and parameters,
// so new transforms do not touch existing code.
class Transform
{
public:
    Transform() = default;
    virtual ~Transform() = default;
    Transform(const Transform&) = delete;
    Transform& operator=(const Transform&) = delete;
    Transform(Transform&&) = delete;
    Transform& operator=(Transform&&) = delete;

    // Stable identifier in files: "direct", "linear", "ranges", "expression".
    [[nodiscard]] virtual QString kind() const = 0;
    // The property value for `value`, checked against `target`. Fails when
    // the value cannot become that property's kind of value.
    [[nodiscard]] virtual Result<PropertyValue> apply(const TagValue& value,
                                                      const PropertyDescriptor& target) const = 0;
    // Parameters, without the kind.
    [[nodiscard]] virtual QJsonObject parameters() const = 0;
    // Short description for the properties panel, such as "0–100 → 0–1".
    [[nodiscard]] virtual QString summary() const = 0;

    // Inverse for operator input. Ranges and expressions have no unique
    // inverse and remain display-only.
    [[nodiscard]] virtual Result<TagValue> inverse(const PropertyValue& value) const;

    // Converts a number, flag or text to the target's kind of value (the
    // direct transform, and the last step of the others).
    [[nodiscard]] static Result<PropertyValue> toProperty(const TagValue& value,
                                                          const PropertyDescriptor& target);
};

using TransformPtr = std::shared_ptr<const Transform>;

class TransformRegistry
{
public:
    using Factory = std::function<Result<TransformPtr>(const QJsonObject& parameters)>;

    [[nodiscard]] static TransformRegistry& instance();
    bool add(const QString& kind, Factory factory);
    [[nodiscard]] Result<TransformPtr> create(const QString& kind,
                                              const QJsonObject& parameters = {}) const;
    [[nodiscard]] QStringList kinds() const;

private:
    TransformRegistry() = default;
    std::map<QString, Factory> m_factories;
};

// Links one bindable property of an object to a tag through a transform.
struct Binding
{
    PropertyId property;
    QString tag;
    TransformPtr transform; // never null; "direct" when nothing else is set

    // The default: the tag's value, converted.
    [[nodiscard]] static TransformPtr direct();

    [[nodiscard]] QJsonObject toJson(const PropertyDescriptor& descriptor) const;
    // `descriptors` are the object's, to find the property by key.
    [[nodiscard]] static Result<Binding> fromJson(const QJsonObject& json,
                                                  std::span<const PropertyDescriptor> descriptors);

    friend bool operator==(const Binding& a, const Binding& b);
};

} // namespace hmi
