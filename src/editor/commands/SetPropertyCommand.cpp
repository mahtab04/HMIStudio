#include "editor/commands/SetPropertyCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

namespace hmi
{

SetPropertyCommand::SetPropertyCommand(Project& project, ScreenId screen,
                                       std::vector<ObjectId> objects, PropertyId property,
                                       PropertyValue value, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_property(property)
    , m_value(std::move(value))
{
    const Screen* s = m_project.screen(m_screen);
    QString name;
    for (const ObjectId& id : objects)
    {
        const HmiObject* object = s ? s->object(id) : nullptr;
        auto before = object ? object->property(property) : std::nullopt;
        if (!before)
            continue;
        m_changes.push_back({id, std::move(*before)});
        if (name.isEmpty())
        {
            if (const PropertyDescriptor* descriptor =
                    findDescriptor(object->descriptors(), property))
                name = QCoreApplication::translate("hmi::Property", descriptor->displayName);
        }
    }
    setText(QCoreApplication::translate("hmi::Commands", "Change %1").arg(name));
}

void SetPropertyCommand::redo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    for (const Change& change : m_changes)
    {
        if (HmiObject* object = screen->object(change.object))
        {
            if (auto set = object->setProperty(m_property, m_value); !set)
                qWarning("SetPropertyCommand: %s", qPrintable(set.error().message));
        }
    }
}

void SetPropertyCommand::undo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    for (const Change& change : m_changes)
    {
        if (HmiObject* object = screen->object(change.object))
        {
            [[maybe_unused]] const auto restored = object->setProperty(m_property, change.before);
            Q_ASSERT(restored);
        }
    }
}

} // namespace hmi
