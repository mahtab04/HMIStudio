#include "editor/app/AppContext.h"

#include "core/model/Project.h"
#include "editor/app/EditorController.h"
#include "editor/app/Theme.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"
#include "editor/tools/ToolContext.h"

namespace hmi
{

AppContext::AppContext(std::unique_ptr<Project> project)
    : m_project(std::move(project))
    , m_theme(std::make_unique<Theme>())
    , m_selection(std::make_unique<SelectionModel>())
    , m_toolContext(
          std::make_unique<ToolContext>(*m_project, m_undoStack, *m_selection, m_inputSettings))
    , m_toolboxDrag(std::make_unique<ToolboxDragController>(*m_toolContext))
    , m_editor(std::make_unique<EditorController>(*m_toolContext, *m_toolboxDrag))
    , m_toolbox(std::make_unique<ToolboxModel>())
{
    Q_ASSERT(m_project && m_project->screenCount() > 0);
    m_toolContext->setScreenId(m_project->screens().front()->id());

    // Objects removed by undo or a later delete leave the selection.
    QObject::connect(&m_project->notifier(), &DocumentNotifier::objectRemoved, m_selection.get(),
                     [this](ScreenId, ObjectId object) { m_selection->remove(object); });
}

// The undo stack holds commands that refer to the project; clear it first.
AppContext::~AppContext()
{
    m_undoStack.clear();
}

} // namespace hmi
