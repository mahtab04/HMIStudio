#pragma once

#include <QAnyStringView>
#include <QByteArray>
#include <QHashFunctions>
#include <QString>
#include <QStringView>
#include <QUuid>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace hmi
{

namespace detail
{

// 32-bit FNV-1a. Used for compile-time ids from string keys; the same function
// runs at run time when a key is read from a file.
[[nodiscard]] constexpr std::uint32_t fnv1a(std::string_view text) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const char c : text)
    {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

} // namespace detail

// Strong id derived from a stable ASCII key ("rectangle", "fillColor").
// Constructed at compile time from a literal, or at run time with fromKey().
// The key text itself lives in the descriptor or registry entry; the id only
// carries the hash, and those tables reject collisions when they are built.
template <typename Tag>
class HashId
{
public:
    constexpr HashId() noexcept = default;

    template <std::size_t N>
    consteval explicit HashId(const char (&key)[N]) noexcept
        : m_value(detail::fnv1a(std::string_view(key, N - 1)))
    {
    }

    [[nodiscard]] static HashId fromKey(QAnyStringView key)
    {
        const QByteArray utf8 = key.toString().toUtf8();
        HashId id;
        id.m_value = detail::fnv1a(std::string_view(utf8.constData(), std::size_t(utf8.size())));
        return id;
    }

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return m_value; }
    [[nodiscard]] constexpr bool isNull() const noexcept { return m_value == 0; }

    friend constexpr auto operator<=>(HashId, HashId) noexcept = default;
    friend constexpr bool operator==(HashId, HashId) noexcept = default;

    friend size_t qHash(HashId id, size_t seed = 0) noexcept { return ::qHash(id.m_value, seed); }

private:
    std::uint32_t m_value = 0;
};

// Strong id for document entities that must stay unique across copy, paste
// and projects (objects, screens). Backed by a random UUID.
template <typename Tag>
class UuidId
{
public:
    UuidId() noexcept = default;

    [[nodiscard]] static UuidId create() { return UuidId(QUuid::createUuid()); }

    [[nodiscard]] static std::optional<UuidId> fromString(QStringView text)
    {
        const QUuid uuid = QUuid::fromString(text);
        if (uuid.isNull())
            return std::nullopt;
        return UuidId(uuid);
    }

    [[nodiscard]] QString toString() const { return m_uuid.toString(QUuid::WithoutBraces); }
    [[nodiscard]] bool isNull() const noexcept { return m_uuid.isNull(); }

    friend bool operator==(const UuidId& a, const UuidId& b) noexcept
    {
        return a.m_uuid == b.m_uuid;
    }

    friend std::strong_ordering operator<=>(const UuidId& a, const UuidId& b) noexcept
    {
        const QUuid& x = a.m_uuid;
        const QUuid& y = b.m_uuid;
        if (const auto c = x.data1 <=> y.data1; c != 0)
            return c;
        if (const auto c = x.data2 <=> y.data2; c != 0)
            return c;
        if (const auto c = x.data3 <=> y.data3; c != 0)
            return c;
        return std::lexicographical_compare_three_way(std::begin(x.data4), std::end(x.data4),
                                                      std::begin(y.data4), std::end(y.data4));
    }

    friend size_t qHash(const UuidId& id, size_t seed = 0) noexcept
    {
        return ::qHash(id.m_uuid, seed);
    }

private:
    explicit UuidId(const QUuid& uuid) noexcept
        : m_uuid(uuid)
    {
    }

    QUuid m_uuid;
};

struct ObjectIdTag;
struct ScreenIdTag;
struct PropertyIdTag;
struct ObjectTypeIdTag;

using ObjectId = UuidId<ObjectIdTag>;
using ScreenId = UuidId<ScreenIdTag>;
using PropertyId = HashId<PropertyIdTag>;
using ObjectTypeId = HashId<ObjectTypeIdTag>;

} // namespace hmi

template <typename Tag>
struct std::hash<hmi::HashId<Tag>>
{
    std::size_t operator()(hmi::HashId<Tag> id) const noexcept { return id.value(); }
};

template <typename Tag>
struct std::hash<hmi::UuidId<Tag>>
{
    std::size_t operator()(const hmi::UuidId<Tag>& id) const noexcept { return qHash(id); }
};
