#include "editor/commands/AlarmCommands.h"

#include "core/model/Project.h"

namespace hmi
{

ChangeAlarmsCommand::ChangeAlarmsCommand(Project& project, AlarmConfiguration after,
                                         const QString& text, QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_before(project.alarms())
    , m_after(std::move(after))
{
}

void ChangeAlarmsCommand::redo()
{
    [[maybe_unused]] const auto set = m_project.setAlarms(m_after);
    Q_ASSERT(set);
}

void ChangeAlarmsCommand::undo()
{
    [[maybe_unused]] const auto set = m_project.setAlarms(m_before);
    Q_ASSERT(set);
}

} // namespace hmi
