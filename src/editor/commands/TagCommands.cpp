#include "editor/commands/TagCommands.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

#include <algorithm>
#include <functional>

namespace hmi
{

ChangeConnectionsCommand::ChangeConnectionsCommand(Project& project, std::vector<Connection> after)
    : m_project(project)
    , m_before(project.connections().begin(), project.connections().end())
    , m_after(std::move(after))
{
    setText(QCoreApplication::translate("hmi::Commands", "Change PLC connections"));
}

void ChangeConnectionsCommand::redo()
{
    [[maybe_unused]] const auto result = m_project.setConnections(m_after);
    Q_ASSERT(result);
}

void ChangeConnectionsCommand::undo()
{
    [[maybe_unused]] const auto result = m_project.setConnections(m_before);
    Q_ASSERT(result);
}

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
    if (m_before.name != m_after.name)
    {
        const std::function<void(const HmiObject&, ScreenId, ObjectId)> collect =
            [&](const HmiObject& object, ScreenId screen, ObjectId root)
        {
            for (const Binding& binding : object.bindings())
            {
                if (binding.tag.compare(m_before.name, Qt::CaseInsensitive) == 0)
                    m_references.push_back(
                        {screen, root, object.id(), binding.property, binding, binding.tag});
            }
            for (const auto& descriptor : object.descriptors())
            {
                if (valueKind(descriptor.defaultValue) != ValueKind::Tag)
                    continue;
                const QString spelling = object.value<TagRef>(descriptor.id).tagName;
                if (spelling.compare(m_before.name, Qt::CaseInsensitive) == 0)
                    m_references.push_back(
                        {screen, root, object.id(), descriptor.id, std::nullopt, spelling});
            }
            for (const EventKind event : kEventKinds)
            {
                const ActionList& actions = object.events().actions(event);
                if (std::ranges::any_of(actions,
                                        [this](const Action& action)
                                        {
                                            return action.usesTag() &&
                                                   action.tag.compare(m_before.name,
                                                                      Qt::CaseInsensitive) == 0;
                                        }))
                    m_eventReferences.push_back({screen, root, object.id(), event, actions});
            }
            for (const auto& child : object.children())
                collect(*child, screen, root);
        };
        for (const auto& screen : project.screens())
            for (const auto& object : screen->objects())
                collect(*object, screen->id(), object->id());
    }
    setText(m_before.name != m_after.name
                ? QCoreApplication::translate("hmi::Commands", "Rename tag %1").arg(m_after.name)
                : QCoreApplication::translate("hmi::Commands", "Change tag %1").arg(m_after.name));
}

void ChangeTagCommand::applyReferences(bool forward)
{
    const std::function<HmiObject*(HmiObject&, ObjectId)> find = [&](HmiObject& object,
                                                                     ObjectId id) -> HmiObject*
    {
        if (object.id() == id)
            return &object;
        for (const auto& child : object.children())
            if (auto* found = find(*child, id))
                return found;
        return nullptr;
    };
    for (const auto& ref : m_references)
    {
        const auto* screen = m_project.screen(ref.screen);
        auto* root = screen ? screen->object(ref.root) : nullptr;
        auto* object = root ? find(*root, ref.object) : nullptr;
        if (!object)
            continue;
        if (ref.binding)
        {
            Binding binding = *ref.binding;
            binding.tag = forward ? m_after.name : ref.spelling;
            [[maybe_unused]] const auto result = object->setBinding(std::move(binding));
            Q_ASSERT(result);
            if (object != root)
                Q_EMIT m_project.notifier().bindingChanged(root->id(), ref.property);
        }
        else
        {
            [[maybe_unused]] const auto result =
                object->setProperty(ref.property, TagRef{forward ? m_after.name : ref.spelling});
            Q_ASSERT(result);
            if (object != root)
                Q_EMIT m_project.notifier().propertyChanged(root->id(), ref.property);
        }
    }
    for (const auto& ref : m_eventReferences)
    {
        const auto* screen = m_project.screen(ref.screen);
        auto* root = screen ? screen->object(ref.root) : nullptr;
        auto* object = root ? find(*root, ref.object) : nullptr;
        if (!object)
            continue;
        ActionList actions = ref.before;
        if (forward)
        {
            for (Action& action : actions)
            {
                if (action.usesTag() && action.tag.compare(m_before.name, Qt::CaseInsensitive) == 0)
                    action.tag = m_after.name;
            }
        }
        object->setActions(ref.event, std::move(actions));
        if (object != root)
            Q_EMIT m_project.notifier().eventsChanged(root->id());
    }
}

void ChangeTagCommand::redo()
{
    if (auto replaced = m_project.tags().replace(m_before.name, m_after); !replaced)
        qWarning("ChangeTagCommand: %s", qPrintable(replaced.error().message));
    applyReferences(true);
}

void ChangeTagCommand::undo()
{
    (void)m_project.tags().replace(m_after.name, m_before);
    applyReferences(false);
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
