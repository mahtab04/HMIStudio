#pragma once

#include "core/model/Events.h"
#include "core/model/Ids.h"

#include <QUndoCommand>

namespace hmi
{

class Project;

// Replaces the action list of one event of an object. The old list is read
// when the command is created.
class SetActionsCommand : public QUndoCommand
{
public:
    SetActionsCommand(Project& project, ScreenId screen, ObjectId object, EventKind event,
                      ActionList after, QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    void apply(const ActionList& actions);

    Project& m_project;
    ScreenId m_screen;
    ObjectId m_object;
    EventKind m_event;
    ActionList m_before;
    ActionList m_after;
};

} // namespace hmi
