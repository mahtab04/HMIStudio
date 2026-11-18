#pragma once

#include "core/alarms/Alarm.h"

#include <QUndoCommand>

namespace hmi
{

class Project;

// Replaces the project's alarms and alarm classes as one undo step; the
// configuration is small, so a whole copy is simplest.
class ChangeAlarmsCommand : public QUndoCommand
{
public:
    ChangeAlarmsCommand(Project& project, AlarmConfiguration after, const QString& text,
                        QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    Project& m_project;
    AlarmConfiguration m_before;
    AlarmConfiguration m_after;
};

} // namespace hmi
