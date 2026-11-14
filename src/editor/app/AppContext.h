#pragma once

#include "quick/input/InputSettings.h"

#include <QUndoStack>

#include <memory>

namespace hmi
{

class EditorController;
class Project;
class SelectionModel;
class Theme;
class ToolContext;
class ToolboxDragController;
class ToolboxModel;

// Owns the application's state and services: the document, undo stack,
// selection, settings, theme and the controllers QML talks to. Created once
// in main(); nothing here is a singleton (ARCHITECTURE.md section 6).
class AppContext
{
public:
    // Takes the document to edit; it must have at least one screen.
    explicit AppContext(std::unique_ptr<Project> project);
    ~AppContext();
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;
    AppContext(AppContext&&) = delete;
    AppContext& operator=(AppContext&&) = delete;

    [[nodiscard]] Project& project() const noexcept { return *m_project; }
    [[nodiscard]] QUndoStack& undoStack() noexcept { return m_undoStack; }
    [[nodiscard]] const InputSettings& inputSettings() const noexcept { return m_inputSettings; }
    [[nodiscard]] SelectionModel& selection() const noexcept { return *m_selection; }
    [[nodiscard]] Theme& theme() const noexcept { return *m_theme; }
    [[nodiscard]] ToolContext& toolContext() const noexcept { return *m_toolContext; }
    [[nodiscard]] EditorController& editor() const noexcept { return *m_editor; }
    [[nodiscard]] ToolboxModel& toolbox() const noexcept { return *m_toolbox; }
    [[nodiscard]] ToolboxDragController& toolboxDrag() const noexcept { return *m_toolboxDrag; }

private:
    InputSettings m_inputSettings;
    QUndoStack m_undoStack;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<Theme> m_theme;
    std::unique_ptr<SelectionModel> m_selection;
    std::unique_ptr<ToolContext> m_toolContext;
    std::unique_ptr<ToolboxDragController> m_toolboxDrag;
    std::unique_ptr<EditorController> m_editor;
    std::unique_ptr<ToolboxModel> m_toolbox;
};

} // namespace hmi
