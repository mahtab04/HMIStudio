#include "editor/commands/ScreenCommands.h"

#include "core/model/Project.h"
#include "core/model/Screen.h"

#include <QCoreApplication>

namespace hmi
{

AddScreenCommand::AddScreenCommand(Project& project, std::unique_ptr<Screen> screen,
                                   std::optional<std::size_t> index, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(std::move(screen))
    , m_index(index)
{
    Q_ASSERT(m_screen);
    m_id = m_screen->id();
    setText(QCoreApplication::translate("hmi::Commands", "Add screen %1").arg(m_screen->name()));
}

AddScreenCommand::~AddScreenCommand() = default;

void AddScreenCommand::redo()
{
    if (!m_screen)
        return;
    auto inserted = m_project.insertScreen(std::move(m_screen), m_index);
    if (!inserted)
    {
        qWarning("AddScreenCommand: %s", qPrintable(inserted.error().message));
        return;
    }
    m_index = m_project.indexOf(m_id);
}

void AddScreenCommand::undo()
{
    auto taken = m_project.takeScreen(m_id);
    if (!taken)
    {
        qWarning("AddScreenCommand: %s", qPrintable(taken.error().message));
        return;
    }
    m_screen = std::move(*taken);
}

RemoveScreenCommand::RemoveScreenCommand(Project& project, ScreenId screen, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_id(screen)
{
    const Screen* s = m_project.screen(screen);
    setText(QCoreApplication::translate("hmi::Commands", "Delete screen %1")
                .arg(s ? s->name() : QString()));
}

RemoveScreenCommand::~RemoveScreenCommand() = default;

void RemoveScreenCommand::redo()
{
    const auto index = m_project.indexOf(m_id);
    if (!index)
        return;
    m_index = *index;
    auto taken = m_project.takeScreen(m_id);
    Q_ASSERT(taken);
    m_screen = std::move(*taken);
}

void RemoveScreenCommand::undo()
{
    if (!m_screen)
        return;
    auto inserted = m_project.insertScreen(std::move(m_screen), m_index);
    if (!inserted)
        qWarning("RemoveScreenCommand: %s", qPrintable(inserted.error().message));
}

ChangeScreenCommand::ChangeScreenCommand(Project& project, ScreenId screen,
                                         const ScreenSettings& after, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_id(screen)
    , m_after(after)
{
    if (const Screen* s = m_project.screen(screen))
        m_before = settingsOf(*s);
    else
        m_before = after;
    const bool renamed = m_before.name != m_after.name;
    setText(
        renamed
            ? QCoreApplication::translate("hmi::Commands", "Rename screen %1").arg(m_after.name)
            : QCoreApplication::translate("hmi::Commands", "Change screen %1").arg(m_after.name));
}

ScreenSettings ChangeScreenCommand::settingsOf(const Screen& screen)
{
    return {screen.name(), screen.size(), screen.backgroundColor()};
}

void ChangeScreenCommand::redo()
{
    apply(m_after);
}

void ChangeScreenCommand::undo()
{
    apply(m_before);
}

void ChangeScreenCommand::apply(const ScreenSettings& settings)
{
    Screen* screen = m_project.screen(m_id);
    if (!screen)
        return;
    screen->setName(settings.name);
    if (auto sized = screen->setSize(settings.size); !sized)
        qWarning("ChangeScreenCommand: %s", qPrintable(sized.error().message));
    if (auto coloured = screen->setBackgroundColor(settings.background); !coloured)
        qWarning("ChangeScreenCommand: %s", qPrintable(coloured.error().message));
}

} // namespace hmi
