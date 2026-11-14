#include "editor/tools/ToolContext.h"

#include "core/model/Project.h"
#include "quick/ICanvasView.h"

namespace hmi
{

ToolContext::ToolContext(Project& project, QUndoStack& undoStack, SelectionModel& selection,
                         const InputSettings& input)
    : m_project(project)
    , m_undoStack(undoStack)
    , m_selection(selection)
    , m_input(input)
{
}

Screen* ToolContext::screen() const
{
    return m_project.screen(m_screenId);
}

double ToolContext::zoom() const
{
    return m_canvas && m_canvas->zoom() > 0.0 ? m_canvas->zoom() : 1.0;
}

double ToolContext::sceneHitTolerance(const PointerGesture& gesture) const
{
    return m_input.hitTolerance(gesture.device) / zoom();
}

double ToolContext::sceneHandleHitSize(const PointerGesture& gesture) const
{
    return m_input.handleHitSize(gesture.device) / zoom();
}

void ToolContext::requestSelectTool() const
{
    if (m_selectToolRequest)
        m_selectToolRequest();
}

} // namespace hmi
