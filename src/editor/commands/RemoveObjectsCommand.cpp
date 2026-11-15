#include "editor/commands/RemoveObjectsCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

#include <algorithm>

namespace hmi
{

RemoveObjectsCommand::RemoveObjectsCommand(Project& project, ScreenId screen,
                                           std::vector<ObjectId> objects, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_objects(std::move(objects))
{
    setText(QCoreApplication::translate("hmi::Commands", "Delete %n object(s)", nullptr,
                                        int(m_objects.size())));
}

RemoveObjectsCommand::~RemoveObjectsCommand() = default;

void RemoveObjectsCommand::redo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    std::vector<std::size_t> indices;
    for (const ObjectId& id : m_objects)
    {
        if (const auto index = screen->indexOf(id))
            indices.push_back(*index);
    }
    std::ranges::sort(indices);

    // Take from the top down so the lower indices stay valid.
    m_removed.resize(indices.size());
    for (std::size_t i = indices.size(); i-- > 0;)
    {
        const ObjectId id = screen->objects()[indices[i]]->id();
        auto taken = screen->takeObject(id);
        Q_ASSERT(taken);
        m_removed[i] = Removed{.index = indices[i], .object = std::move(*taken)};
    }
}

void RemoveObjectsCommand::undo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    // Ascending order: each object goes back below everything that was above
    // it originally.
    for (Removed& removed : m_removed)
    {
        auto inserted = screen->insertObject(std::move(removed.object), removed.index);
        if (!inserted)
            qWarning("RemoveObjectsCommand: %s", qPrintable(inserted.error().message));
    }
    m_removed.clear();
}

} // namespace hmi
