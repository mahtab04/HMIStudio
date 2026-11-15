#include "editor/commands/SetPropertyCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

namespace hmi
{

SetPropertyCommand::SetPropertyCommand(Project& project, ScreenId screen,
                                       std::vector<ObjectId> objects, PropertyId property,
                                       PropertyValue value, int editSession, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_property(property)
    , m_value(std::move(value))
    , m_editSession(editSession)
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

namespace
{
constexpr int kSetPropertyId = 0x50524F50; // "PROP"
} // namespace

int SetPropertyCommand::id() const
{
    return m_editSession != 0 ? kSetPropertyId : -1;
}

bool SetPropertyCommand::mergeWith(const QUndoCommand* other)
{
    const auto* next = dynamic_cast<const SetPropertyCommand*>(other);
    if (!next || next->m_editSession != m_editSession || next->m_screen != m_screen ||
        next->m_property != m_property || next->m_changes.size() != m_changes.size())
        return false;
    for (std::size_t i = 0; i < m_changes.size(); ++i)
    {
        if (next->m_changes[i].object != m_changes[i].object)
            return false;
    }
    // Keep this command's old values and take the newest value.
    m_value = next->m_value;
    return true;
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
