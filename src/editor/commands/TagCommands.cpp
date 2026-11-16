#include "editor/commands/TagCommands.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

namespace hmi
{

AddTagCommand::AddTagCommand(Project& project, Tag tag, std::optional<std::size_t> index,
                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_tag(std::move(tag))
    , m_index(index)
{
    setText(QCoreApplication::translate("hmi::Commands", "Add tag %1").arg(m_tag.name));
}

void AddTagCommand::redo()
{
    if (auto added = m_project.tags().insert(m_tag, m_index); !added)
        qWarning("AddTagCommand: %s", qPrintable(added.error().message));
}

void AddTagCommand::undo()
{
    (void)m_project.tags().take(m_tag.name);
}

RemoveTagCommand::RemoveTagCommand(Project& project, const QString& name, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_name(name)
{
    setText(QCoreApplication::translate("hmi::Commands", "Delete tag %1").arg(name));
}

void RemoveTagCommand::redo()
{
    m_index = m_project.tags().indexOf(m_name).value_or(0);
    if (auto taken = m_project.tags().take(m_name))
        m_tag = std::move(*taken);
}

void RemoveTagCommand::undo()
{
    if (auto added = m_project.tags().insert(m_tag, m_index); !added)
        qWarning("RemoveTagCommand: %s", qPrintable(added.error().message));
}

ChangeTagCommand::ChangeTagCommand(Project& project, const QString& name, Tag after,
                                   QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_after(std::move(after))
{
    if (const Tag* tag = m_project.tags().find(name))
        m_before = *tag;
    setText(m_before.name != m_after.name
                ? QCoreApplication::translate("hmi::Commands", "Rename tag %1").arg(m_after.name)
                : QCoreApplication::translate("hmi::Commands", "Change tag %1").arg(m_after.name));
}

void ChangeTagCommand::redo()
{
    if (auto replaced = m_project.tags().replace(m_before.name, m_after); !replaced)
        qWarning("ChangeTagCommand: %s", qPrintable(replaced.error().message));
}

void ChangeTagCommand::undo()
{
    (void)m_project.tags().replace(m_after.name, m_before);
}

SetBindingCommand::SetBindingCommand(Project& project, ScreenId screen,
                                     std::vector<ObjectId> objects, PropertyId property,
                                     std::optional<Binding> binding, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_property(property)
    , m_after(std::move(binding))
{
    const Screen* s = m_project.screen(screen);
    for (const ObjectId& id : objects)
    {
        const HmiObject* object = s ? s->object(id) : nullptr;
        if (!object)
            continue;
        const Binding* before = object->binding(property);
        m_changes.push_back({id, before ? std::optional(*before) : std::nullopt});
    }
    setText(m_after ? QCoreApplication::translate("hmi::Commands", "Bind to %1").arg(m_after->tag)
                    : QCoreApplication::translate("hmi::Commands", "Remove tag binding"));
}

void SetBindingCommand::apply(ObjectId id, const std::optional<Binding>& binding)
{
    Screen* screen = m_project.screen(m_screen);
    HmiObject* object = screen ? screen->object(id) : nullptr;
    if (!object)
        return;
    if (binding)
    {
        if (auto set = object->setBinding(*binding); !set)
            qWarning("SetBindingCommand: %s", qPrintable(set.error().message));
    }
    else
    {
        object->removeBinding(m_property);
    }
}

void SetBindingCommand::redo()
{
    for (const Change& change : m_changes)
        apply(change.object, m_after);
}

void SetBindingCommand::undo()
{
    for (const Change& change : m_changes)
        apply(change.object, change.before);
}

} // namespace hmi
