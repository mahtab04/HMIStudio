#include "core/registry/ObjectRegistry.h"

#include "core/model/HmiObject.h"

#include <algorithm>
#include <tuple>

namespace hmi
{

ObjectRegistry& ObjectRegistry::instance()
{
    static ObjectRegistry registry;
    return registry;
}

bool ObjectRegistry::add(ObjectTypeInfo info)
{
    if (!info.create || info.id != ObjectTypeId::fromKey(info.key) || find(info.id))
    {
        qWarning("ObjectRegistry: rejected type '%s' (duplicate id or key mismatch)",
                 qPrintable(info.key));
        return false;
    }

    const auto order = [](const ObjectTypeInfo& t)
    {
        return std::tie(t.category, t.key);
    };
    const auto at = std::ranges::upper_bound(m_types, order(info), std::less<>{}, order);
    m_types.insert(at, std::move(info));
    return true;
}

const ObjectTypeInfo* ObjectRegistry::find(ObjectTypeId id) const noexcept
{
    const auto it = std::ranges::find(m_types, id, &ObjectTypeInfo::id);
    return it == m_types.end() ? nullptr : &*it;
}

const ObjectTypeInfo* ObjectRegistry::findByKey(QStringView key) const noexcept
{
    const auto it = std::ranges::find_if(m_types, [key](const auto& t) { return t.key == key; });
    return it == m_types.end() ? nullptr : &*it;
}

std::unique_ptr<HmiObject> ObjectRegistry::create(ObjectTypeId id) const
{
    const ObjectTypeInfo* info = find(id);
    return info ? info->create() : nullptr;
}

std::vector<const ObjectTypeInfo*> ObjectRegistry::types(ObjectCategory category) const
{
    std::vector<const ObjectTypeInfo*> result;
    for (const ObjectTypeInfo& t : m_types)
    {
        if (t.category == category)
            result.push_back(&t);
    }
    return result;
}

} // namespace hmi
