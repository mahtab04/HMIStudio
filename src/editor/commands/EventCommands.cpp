#include "editor/commands/EventCommands.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <QCoreApplication>

namespace hmi
{

SetActionsCommand::SetActionsCommand(Project& project, ScreenId screen, ObjectId object,
                                     EventKind event, ActionList after, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_object(object)
    , m_event(event)
    , m_after(std::move(after))
{
    const Screen* s = m_project.screen(screen);
    if (const HmiObject* target = s ? s->object(object) : nullptr)
        m_before = target->events().actions(event);
    setText(QCoreApplication::translate("hmi::Commands", "Change %1 actions")
                .arg(Events::displayName(event)));
}

void SetActionsCommand::apply(const ActionList& actions)
{
    Screen* screen = m_project.screen(m_screen);
    if (HmiObject* object = screen ? screen->object(m_object) : nullptr)
        object->setActions(m_event, actions);
}

void SetActionsCommand::redo()
{
    apply(m_after);
}

void SetActionsCommand::undo()
{
    apply(m_before);
}

} // namespace hmi
