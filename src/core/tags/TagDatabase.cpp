#include "core/tags/TagDatabase.h"

#include "core/model/DocumentNotifier.h"

#include <algorithm>

namespace hmi
{

std::optional<std::size_t> TagDatabase::indexOf(const QString& name) const
{
    const auto it =
        std::ranges::find_if(m_tags, [&name](const Tag& tag)
                             { return tag.name.compare(name, Qt::CaseInsensitive) == 0; });
    if (it == m_tags.end())
        return std::nullopt;
    return std::size_t(it - m_tags.begin());
}

const Tag* TagDatabase::find(const QString& name) const
{
    const auto index = indexOf(name);
    return index ? &m_tags[*index] : nullptr;
}

Result<void> TagDatabase::insert(Tag tag, std::optional<std::size_t> index)
{
    if (!Tag::isValidName(tag.name))
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("'%1' is not a valid tag name").arg(tag.name));
    if (indexOf(tag.name))
        return makeError(ErrorCode::Duplicate,
                         QStringLiteral("A tag called '%1' already exists").arg(tag.name));
    const std::size_t at = std::min(index.value_or(m_tags.size()), m_tags.size());
    const QString name = tag.name;
    m_tags.insert(m_tags.begin() + std::ptrdiff_t(at), std::move(tag));
    if (m_notifier)
        Q_EMIT m_notifier->tagAdded(name);
    return {};
}

Result<Tag> TagDatabase::take(const QString& name)
{
    const auto index = indexOf(name);
    if (!index)
        return makeError(ErrorCode::NotFound, QStringLiteral("No tag called '%1'").arg(name));
    Tag tag = std::move(m_tags[*index]);
    m_tags.erase(m_tags.begin() + std::ptrdiff_t(*index));
    if (m_notifier)
        Q_EMIT m_notifier->tagRemoved(tag.name);
    return tag;
}

Result<void> TagDatabase::replace(const QString& name, Tag tag)
{
    const auto index = indexOf(name);
    if (!index)
        return makeError(ErrorCode::NotFound, QStringLiteral("No tag called '%1'").arg(name));
    if (!Tag::isValidName(tag.name))
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("'%1' is not a valid tag name").arg(tag.name));
    if (const auto other = indexOf(tag.name); other && *other != *index)
        return makeError(ErrorCode::Duplicate,
                         QStringLiteral("A tag called '%1' already exists").arg(tag.name));
    const QString oldName = m_tags[*index].name;
    m_tags[*index] = std::move(tag);
    if (m_notifier)
        Q_EMIT m_notifier->tagChanged(oldName, m_tags[*index].name);
    return {};
}

QString TagDatabase::uniqueName(const QString& base) const
{
    const QString stem = base.trimmed().isEmpty() ? QStringLiteral("Tag") : base.trimmed();
    if (!indexOf(stem))
        return stem;
    for (int n = 2;; ++n)
    {
        const QString candidate = QStringLiteral("%1_%2").arg(stem).arg(n);
        if (!indexOf(candidate))
            return candidate;
    }
}

void TagDatabase::takeFrom(TagDatabase& other)
{
    m_tags = std::move(other.m_tags);
    other.m_tags.clear();
}

} // namespace hmi
