#pragma once

#include "core/model/Ids.h"

#include <QColor>
#include <QSizeF>
#include <QString>
#include <QUndoCommand>

#include <memory>
#include <optional>

namespace hmi
{

class Project;
class Screen;

// Adds a screen to the project. While undone, the command owns it.
class AddScreenCommand : public QUndoCommand
{
public:
    AddScreenCommand(Project& project, std::unique_ptr<Screen> screen,
                     std::optional<std::size_t> index = std::nullopt,
                     QUndoCommand* parent = nullptr);
    ~AddScreenCommand() override;
    AddScreenCommand(const AddScreenCommand&) = delete;
    AddScreenCommand& operator=(const AddScreenCommand&) = delete;
    AddScreenCommand(AddScreenCommand&&) = delete;
    AddScreenCommand& operator=(AddScreenCommand&&) = delete;

    [[nodiscard]] ScreenId screenId() const noexcept { return m_id; }

    void redo() override;
    void undo() override;

private:
    Project& m_project;
    ScreenId m_id;
    std::unique_ptr<Screen> m_screen;
    std::optional<std::size_t> m_index;
};

// Removes a screen with its objects. While done, the command owns it and
// remembers its position.
class RemoveScreenCommand : public QUndoCommand
{
public:
    RemoveScreenCommand(Project& project, ScreenId screen, QUndoCommand* parent = nullptr);
    ~RemoveScreenCommand() override;
    RemoveScreenCommand(const RemoveScreenCommand&) = delete;
    RemoveScreenCommand& operator=(const RemoveScreenCommand&) = delete;
    RemoveScreenCommand(RemoveScreenCommand&&) = delete;
    RemoveScreenCommand& operator=(RemoveScreenCommand&&) = delete;

    void redo() override;
    void undo() override;

private:
    Project& m_project;
    ScreenId m_id;
    std::unique_ptr<Screen> m_screen;
    std::size_t m_index = 0;
};

// The settings of a screen itself: name, size and background colour.
struct ScreenSettings
{
    QString name;
    QSizeF size;
    QColor background;
    friend bool operator==(const ScreenSettings&, const ScreenSettings&) = default;
};

// Changes a screen's settings (rename, resize, recolour). The old settings
// are read when the command is created.
class ChangeScreenCommand : public QUndoCommand
{
public:
    ChangeScreenCommand(Project& project, ScreenId screen, const ScreenSettings& after,
                        QUndoCommand* parent = nullptr);

    [[nodiscard]] static ScreenSettings settingsOf(const Screen& screen);

    void redo() override;
    void undo() override;

private:
    void apply(const ScreenSettings& settings);

    Project& m_project;
    ScreenId m_id;
    ScreenSettings m_before;
    ScreenSettings m_after;
};

} // namespace hmi
