#include "editor/commands/ReorderCommand.h"

#include "core/model/Project.h"

namespace hmi
{

ReorderCommand::ReorderCommand(Project& project, ScreenId screen, std::vector<ObjectId> before,
                               std::vector<ObjectId> after, const QString& text,
                               QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_project(project)
    , m_screen(screen)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
}

void ReorderCommand::redo()
{
    apply(m_after);
}

void ReorderCommand::undo()
{
    apply(m_before);
}

void ReorderCommand::apply(const std::vector<ObjectId>& order)
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    if (auto set = screen->setOrder(order); !set)
        qWarning("ReorderCommand: %s", qPrintable(set.error().message));
}

} // namespace hmi
