#include "editor/app/AppContext.h"

#include "comm/TagEngine.h"
#include "core/model/Project.h"
#include "editor/app/AppSettings.h"
#include "editor/app/DocumentController.h"
#include "editor/app/EditActions.h"
#include "editor/app/EditorController.h"
#include "editor/app/SimulationController.h"
#include "editor/app/Theme.h"
#include "editor/models/LayerModel.h"
#include "editor/models/PropertyModel.h"
#include "editor/models/ScreenListModel.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/TagModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"
#include "editor/tools/ToolContext.h"
#include "render/GraphicLibrary.h"

#include <QDir>
#include <QStandardPaths>

namespace hmi
{

AppContext::AppContext(std::unique_ptr<Project> project, const Paths& paths)
    : m_project(std::move(project))
    , m_graphics(std::make_unique<GraphicLibrary>(m_project->assets()))
    , m_theme(std::make_unique<Theme>())
    , m_selection(std::make_unique<SelectionModel>())
    , m_tagEngine(std::make_unique<TagEngine>())
    , m_toolContext(
          std::make_unique<ToolContext>(*m_project, m_undoStack, *m_selection, m_inputSettings))
    , m_editActions(std::make_unique<EditActions>(*m_toolContext))
    , m_toolboxDrag(std::make_unique<ToolboxDragController>(*m_toolContext))
    , m_editor(std::make_unique<EditorController>(*m_toolContext, *m_editActions, *m_toolboxDrag))
    , m_toolbox(std::make_unique<ToolboxModel>())
    , m_settings(std::make_unique<AppSettings>(paths.settingsFile))
{
    Q_ASSERT(m_project && m_project->screenCount() > 0);
    m_toolContext->setScreenId(m_project->screens().front()->id());
    m_toolContext->setGraphics(m_graphics.get());

    const QString autosave =
        !paths.autosaveFolder.isEmpty()
            ? paths.autosaveFolder
            : QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                  .filePath(QStringLiteral("autosave"));
    m_document =
        std::make_unique<DocumentController>(*m_project, m_undoStack, *m_settings, autosave);
    m_properties = std::make_unique<PropertyModel>(*m_toolContext);
    m_layers = std::make_unique<LayerModel>(*m_toolContext);
    m_screens = std::make_unique<ScreenListModel>(*m_toolContext);
    m_tags = std::make_unique<TagModel>(*m_toolContext, *m_tagEngine);
    m_simulation = std::make_unique<SimulationController>(*m_toolContext, *m_tagEngine);

    // Models follow the screen the editor shows.
    QObject::connect(m_editor.get(), &EditorController::currentScreenChanged, m_properties.get(),
                     &PropertyModel::refresh);
    QObject::connect(m_editor.get(), &EditorController::currentScreenChanged, m_layers.get(),
                     &LayerModel::refresh);
    QObject::connect(m_editor.get(), &EditorController::currentScreenChanged, m_screens.get(),
                     &ScreenListModel::currentChanged);

    // The input density chosen last time, else the one the hardware suggests.
    if (const int density = m_settings->density(); density >= 0)
        m_theme->setDensity(static_cast<Theme::Density>(density));
    QObject::connect(m_theme.get(), &Theme::densityChanged, m_settings.get(),
                     [this] { m_settings->setDensity(int(m_theme->density())); });

    // Objects removed by undo or a later delete leave the selection.
    QObject::connect(&m_project->notifier(), &DocumentNotifier::objectRemoved, m_selection.get(),
                     [this](ScreenId, ObjectId object) { m_selection->remove(object); });
}

// The undo stack holds commands that refer to the project; clear it first.
AppContext::~AppContext()
{
    m_undoStack.clear();
    // Models listen to the project; they go before it.
    m_simulation.reset();
    m_tags.reset();
    m_screens.reset();
    m_layers.reset();
    m_properties.reset();
    m_document.reset();
}

} // namespace hmi
