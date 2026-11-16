#pragma once

#include "quick/input/InputSettings.h"

#include <QString>
#include <QUndoStack>

#include <memory>

namespace hmi
{

class AppSettings;
class DocumentController;
class EditActions;
class LayerModel;
class PropertyModel;
class ScreenListModel;
class EditorController;
class GraphicLibrary;
class Project;
class SelectionModel;
class SimulationController;
class TagEngine;
class TagModel;
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
    // Where settings and autosave copies go. Empty means the platform's
    // standard locations; tests pass temporary ones.
    struct Paths
    {
        QString settingsFile;
        QString autosaveFolder;
    };

    // Takes the document to edit; it must have at least one screen.
    explicit AppContext(std::unique_ptr<Project> project, const Paths& paths = {});
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
    [[nodiscard]] EditActions& editActions() const noexcept { return *m_editActions; }
    [[nodiscard]] EditorController& editor() const noexcept { return *m_editor; }
    [[nodiscard]] ToolboxModel& toolbox() const noexcept { return *m_toolbox; }
    [[nodiscard]] ToolboxDragController& toolboxDrag() const noexcept { return *m_toolboxDrag; }
    [[nodiscard]] AppSettings& settings() const noexcept { return *m_settings; }
    [[nodiscard]] GraphicLibrary& graphics() const noexcept { return *m_graphics; }
    [[nodiscard]] DocumentController& document() const noexcept { return *m_document; }
    [[nodiscard]] PropertyModel& properties() const noexcept { return *m_properties; }
    [[nodiscard]] LayerModel& layers() const noexcept { return *m_layers; }
    [[nodiscard]] ScreenListModel& screens() const noexcept { return *m_screens; }
    [[nodiscard]] TagEngine& tagEngine() const noexcept { return *m_tagEngine; }
    [[nodiscard]] TagModel& tags() const noexcept { return *m_tags; }
    [[nodiscard]] SimulationController& simulation() const noexcept { return *m_simulation; }

private:
    InputSettings m_inputSettings;
    QUndoStack m_undoStack;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<GraphicLibrary> m_graphics;
    std::unique_ptr<Theme> m_theme;
    std::unique_ptr<SelectionModel> m_selection;
    std::unique_ptr<TagEngine> m_tagEngine;
    std::unique_ptr<ToolContext> m_toolContext;
    std::unique_ptr<EditActions> m_editActions;
    std::unique_ptr<ToolboxDragController> m_toolboxDrag;
    std::unique_ptr<EditorController> m_editor;
    std::unique_ptr<ToolboxModel> m_toolbox;
    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<DocumentController> m_document;
    std::unique_ptr<PropertyModel> m_properties;
    std::unique_ptr<LayerModel> m_layers;
    std::unique_ptr<ScreenListModel> m_screens;
    std::unique_ptr<TagModel> m_tags;
    std::unique_ptr<SimulationController> m_simulation;
};

} // namespace hmi
