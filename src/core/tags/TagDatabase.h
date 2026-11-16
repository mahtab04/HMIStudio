#pragma once

#include "core/Result.h"
#include "core/tags/Tag.h"

#include <optional>
#include <span>
#include <vector>

namespace hmi
{

class DocumentNotifier;

// The project's tags, in the order the user sees them. Names are unique
// regardless of case. Inside a Project every change is announced through the
// DocumentNotifier (tagAdded, tagRemoved, tagChanged); changes go through
// commands like every other document change.
class TagDatabase
{
public:
    TagDatabase() = default;
    TagDatabase(const TagDatabase&) = delete;
    TagDatabase& operator=(const TagDatabase&) = delete;
    TagDatabase(TagDatabase&&) = delete;
    TagDatabase& operator=(TagDatabase&&) = delete;

    [[nodiscard]] std::span<const Tag> tags() const noexcept { return m_tags; }
    [[nodiscard]] std::size_t size() const noexcept { return m_tags.size(); }
    [[nodiscard]] const Tag* find(const QString& name) const;
    [[nodiscard]] std::optional<std::size_t> indexOf(const QString& name) const;

    // Adds at `index` (default: last). Fails for an invalid or taken name.
    Result<void> insert(Tag tag, std::optional<std::size_t> index = std::nullopt);
    // Removes and returns the tag, for undo.
    Result<Tag> take(const QString& name);
    // Replaces the tag called `name` (which may be renamed by `tag`).
    Result<void> replace(const QString& name, Tag tag);

    // A name not used yet: `base`, else `base_2`, `base_3`, ...
    [[nodiscard]] QString uniqueName(const QString& base) const;

    // Takes over `other`'s tags (when a project is replaced); no signals.
    void takeFrom(TagDatabase& other);

private:
    friend class Project;
    void setNotifier(DocumentNotifier* notifier) noexcept { m_notifier = notifier; }

    std::vector<Tag> m_tags;
    DocumentNotifier* m_notifier = nullptr;
};

} // namespace hmi
