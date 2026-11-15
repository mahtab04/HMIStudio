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
    return m_set.contains(id);
}

void SelectionModel::set(std::vector<ObjectId> ids)
{
    // Drop duplicates, keeping the first occurrence.
    std::vector<ObjectId> unique;
    unique.reserve(ids.size());
    QSet<ObjectId> seen;
    seen.reserve(qsizetype(ids.size()));
    for (const ObjectId& id : ids)
    {
        if (!seen.contains(id))
        {
            seen.insert(id);
            unique.push_back(id);
        }
    }
    if (unique == m_ids)
        return;
    m_ids = std::move(unique);
    m_set = std::move(seen);
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
    m_set.insert(id);
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
    if (!m_set.remove(id))
        return;
    std::erase(m_ids, id);
    Q_EMIT changed();
}

void SelectionModel::clear()
{
    if (m_ids.empty())
        return;
    m_ids.clear();
    m_set.clear();
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
