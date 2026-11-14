#include "editor/models/SelectionModel.h"

#include <algorithm>

namespace hmi
{

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
{
}

bool SelectionModel::contains(ObjectId id) const
{
    return std::ranges::find(m_ids, id) != m_ids.end();
}

void SelectionModel::set(std::vector<ObjectId> ids)
{
    // Drop duplicates, keeping the first occurrence.
    std::vector<ObjectId> unique;
    unique.reserve(ids.size());
    for (const ObjectId& id : ids)
    {
        if (std::ranges::find(unique, id) == unique.end())
            unique.push_back(id);
    }
    if (unique == m_ids)
        return;
    m_ids = std::move(unique);
    Q_EMIT changed();
}

void SelectionModel::selectOnly(ObjectId id)
{
    set({id});
}

void SelectionModel::add(ObjectId id)
{
    if (contains(id))
        return;
    m_ids.push_back(id);
    Q_EMIT changed();
}

void SelectionModel::toggle(ObjectId id)
{
    if (contains(id))
        remove(id);
    else
        add(id);
}

void SelectionModel::remove(ObjectId id)
{
    const auto removed = std::erase(m_ids, id);
    if (removed > 0)
        Q_EMIT changed();
}

void SelectionModel::clear()
{
    if (m_ids.empty())
        return;
    m_ids.clear();
    Q_EMIT changed();
}

void SelectionModel::setMultiSelectMode(bool enabled)
{
    if (enabled == m_multiSelectMode)
        return;
    m_multiSelectMode = enabled;
    Q_EMIT multiSelectModeChanged();
}

} // namespace hmi
