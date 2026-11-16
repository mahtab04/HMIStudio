#include "editor/commands/ChangeStyleCommand.h"

#include "core/model/Project.h"

#include <QCoreApplication>

namespace hmi
{

ChangeStyleCommand::ChangeStyleCommand(Project& project, const HmiStyle& after,
                                       QUndoCommand* parent)
    : QUndoCommand(QCoreApplication::translate("hmi::Commands", "Change project style"), parent)
    , m_project(project)
    , m_before(project.style())
    , m_after(after)
{
}

void ChangeStyleCommand::redo()
{
    m_project.setStyle(m_after);
}

void ChangeStyleCommand::undo()
{
    m_project.setStyle(m_before);
}

} // namespace hmi
