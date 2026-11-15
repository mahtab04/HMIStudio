#include "editor/commands/UngroupCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

#include <algorithm>

namespace hmi
{

UngroupCommand::UngroupCommand(Project& project, ScreenId screen, std::vector<ObjectId> groups,
                               QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_groups(std::move(groups))
{
    setText(QCoreApplication::translate("hmi::Commands", "Ungroup"));
}

UngroupCommand::~UngroupCommand() = default;

void UngroupCommand::redo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    std::vector<std::size_t> indices;
    for (const ObjectId& id : m_groups)
    {
        const auto index = screen->indexOf(id);
        if (index && !screen->objects()[*index]->children().empty())
            indices.push_back(*index);
    }
    std::ranges::sort(indices);

    // From the top down, so each group's index is still valid when it is
    // replaced.
    m_ungrouped.resize(indices.size());
    for (std::size_t i = indices.size(); i-- > 0;)
    {
        auto taken = screen->takeObject(screen->objects()[indices[i]]->id());
        Q_ASSERT(taken);
        Ungrouped& entry = m_ungrouped[i];
        entry.index = indices[i];
        entry.group = std::move(*taken);
        entry.children.clear();

        std::size_t at = entry.index;
        for (auto& child : entry.group->ungroupedChildren())
        {
            entry.children.push_back(child->id());
            auto inserted = screen->insertObject(std::move(child), at++);
            if (!inserted)
                qWarning("UngroupCommand: %s", qPrintable(inserted.error().message));
        }
    }

    m_childIds.clear();
    for (const Ungrouped& entry : m_ungrouped)
        m_childIds.insert(m_childIds.end(), entry.children.begin(), entry.children.end());
}

void UngroupCommand::undo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    // Lowest group first: the children of the groups above it then sit at
    // their groups' original indices again.
    for (Ungrouped& entry : m_ungrouped)
    {
        for (const ObjectId& child : entry.children)
        {
            [[maybe_unused]] auto taken = screen->takeObject(child);
            Q_ASSERT(taken);
        }
        auto inserted = screen->insertObject(std::move(entry.group), entry.index);
        if (!inserted)
            qWarning("UngroupCommand: %s", qPrintable(inserted.error().message));
    }
    m_ungrouped.clear();
}

} // namespace hmi
