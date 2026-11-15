#include "editor/commands/GroupObjectsCommand.h"

#include "core/model/GroupObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

#include <algorithm>

namespace hmi
{

GroupObjectsCommand::GroupObjectsCommand(Project& project, ScreenId screen,
                                         std::vector<ObjectId> objects, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_objects(std::move(objects))
{
    setText(QCoreApplication::translate("hmi::Commands", "Group %n object(s)", nullptr,
                                        int(m_objects.size())));
}

GroupObjectsCommand::~GroupObjectsCommand() = default;

void GroupObjectsCommand::redo()
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
    if (indices.empty())
        return;
    std::ranges::sort(indices);

    m_originals.resize(indices.size());
    for (std::size_t i = indices.size(); i-- > 0;)
    {
        auto taken = screen->takeObject(screen->objects()[indices[i]]->id());
        Q_ASSERT(taken);
        m_originals[i] = Original{.index = indices[i], .object = std::move(*taken)};
    }

    std::vector<std::unique_ptr<HmiObject>> children;
    children.reserve(m_originals.size());
    for (const Original& original : m_originals)
        children.push_back(original.object->clone());
    // The topmost object's slot, after the ones below it were removed.
    const std::size_t at = indices.back() - (indices.size() - 1);
    auto inserted = screen->insertObject(GroupObject::create(std::move(children), m_groupId), at);
    if (!inserted)
        qWarning("GroupObjectsCommand: %s", qPrintable(inserted.error().message));
}

void GroupObjectsCommand::undo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen || m_originals.empty())
        return;
    [[maybe_unused]] auto group = screen->takeObject(m_groupId);
    Q_ASSERT(group);
    for (Original& original : m_originals)
    {
        auto inserted = screen->insertObject(std::move(original.object), original.index);
        if (!inserted)
            qWarning("GroupObjectsCommand: %s", qPrintable(inserted.error().message));
    }
    m_originals.clear();
}

} // namespace hmi
