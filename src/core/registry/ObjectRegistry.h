#pragma once

#include "core/model/Ids.h"

#include <QString>

#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace hmi
{

class HmiObject;

// Toolbox section an object type appears in.
enum class ObjectCategory
{
    BasicObjects,
    Elements,
    Controls,
    ProcessSymbols,
};

// What the registry knows about one object type: how to create it and how to
// show it in the toolbox. `displayName` is translated in the
// "hmi::ObjectRegistry" context.
struct ObjectTypeInfo
{
    ObjectTypeId id;
    QString key;
    const char* displayName = nullptr;
    ObjectCategory category = ObjectCategory::BasicObjects;
    QString iconPath;
    std::function<std::unique_ptr<HmiObject>()> create;
};

// Factory for HMI object types (Factory Method / registry pattern). Each type
// registers itself at start-up with HMI_REGISTER_OBJECT; afterwards the
// registry is read-only, which is why it may be a function-local static.
class ObjectRegistry
{
public:
    [[nodiscard]] static ObjectRegistry& instance();

    // Adds a type. Returns false, and keeps the existing entry, if the id is
    // already registered or the key does not hash to the id.
    bool add(ObjectTypeInfo info);

    [[nodiscard]] const ObjectTypeInfo* find(ObjectTypeId id) const noexcept;
    [[nodiscard]] const ObjectTypeInfo* findByKey(QStringView key) const noexcept;

    // New object of the type with default properties, or nullptr if unknown.
    [[nodiscard]] std::unique_ptr<HmiObject> create(ObjectTypeId id) const;

    // All types, sorted by category then key, so the order does not depend
    // on static initialization order.
    [[nodiscard]] std::span<const ObjectTypeInfo> types() const noexcept { return m_types; }

    [[nodiscard]] std::vector<const ObjectTypeInfo*> types(ObjectCategory category) const;

private:
    ObjectRegistry() = default;

    std::vector<ObjectTypeInfo> m_types;
};

} // namespace hmi

#define HMI_DETAIL_CONCAT2(a, b) a##b
#define HMI_DETAIL_CONCAT(a, b) HMI_DETAIL_CONCAT2(a, b)

// Registers an HmiObject subclass with the ObjectRegistry. Place at the
// bottom of the type's .cpp file, inside no namespace. `Class::kTypeId` must
// be built from the same key.
#define HMI_REGISTER_OBJECT(Class, Key, DisplayName, Category, IconPath)                           \
    static_assert(Class::kTypeId == ::hmi::ObjectTypeId{Key}, "kTypeId must match the key");       \
    namespace                                                                                      \
    {                                                                                              \
    [[maybe_unused]] const bool HMI_DETAIL_CONCAT(                                                 \
        hmiRegistered, __LINE__) = ::hmi::ObjectRegistry::instance().add(::hmi::ObjectTypeInfo{    \
        .id = Class::kTypeId,                                                                      \
        .key = QStringLiteral(Key),                                                                \
        .displayName = DisplayName,                                                                \
        .category = Category,                                                                      \
        .iconPath = QStringLiteral(IconPath),                                                      \
        .create = [] { return std::unique_ptr<::hmi::HmiObject>(std::make_unique<Class>()); }});   \
    }
