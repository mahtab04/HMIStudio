#pragma once

#include "core/model/Ids.h"
#include "core/spatial/Snapper.h"
#include "quick/input/InputSettings.h"
#include "quick/input/PointerGesture.h"

#include <QPointF>

#include <functional>

class QUndoStack;

namespace hmi
{

class ICanvasView;
class Project;
class Screen;
class SelectionModel;

// Everything a tool may use: the document and current screen (read, and
// changed only through commands on the undo stack), the selection, the input
// thresholds and the canvas view for previews and navigation.
class ToolContext
{
public:
    ToolContext(Project& project, QUndoStack& undoStack, SelectionModel& selection,
                const InputSettings& input);

    [[nodiscard]] Project& project() const noexcept { return m_project; }
    [[nodiscard]] ScreenId screenId() const noexcept { return m_screenId; }
    void setScreenId(ScreenId id) noexcept { m_screenId = id; }
    [[nodiscard]] Screen* screen() const;

    [[nodiscard]] QUndoStack& undoStack() const noexcept { return m_undoStack; }
    [[nodiscard]] SelectionModel& selection() const noexcept { return m_selection; }
    [[nodiscard]] const InputSettings& input() const noexcept { return m_input; }

    [[nodiscard]] ICanvasView* canvas() const noexcept { return m_canvas; }
    void setCanvas(ICanvasView* canvas) noexcept { m_canvas = canvas; }

    // Device hit tolerance and handle hit size converted to scene units at
    // the current zoom.
    [[nodiscard]] double sceneHitTolerance(const PointerGesture& gesture) const;
    [[nodiscard]] double sceneHandleHitSize(const PointerGesture& gesture) const;
    [[nodiscard]] double zoom() const;

    // Asks the tool controller to switch back to the select tool (after a
    // placement, or Escape in a placement tool).
    void requestSelectTool() const;
    void setSelectToolRequest(std::function<void()> request)
    {
        m_selectToolRequest = std::move(request);
    }

    // Asks the shell to open the context menu for the selection at a view
    // position (right-click).
    void requestContextMenu(QPointF viewPos) const;
    void setContextMenuRequest(std::function<void(QPointF)> request)
    {
        m_contextMenuRequest = std::move(request);
    }

    // Snapping while moving and resizing. Off, or with Alt held, objects
    // follow the pointer to the whole scene unit.
    [[nodiscard]] bool snapEnabled() const noexcept { return m_snapEnabled; }
    void setSnapEnabled(bool enabled) noexcept { m_snapEnabled = enabled; }
    [[nodiscard]] const Snapper::Settings& snapSettings() const noexcept { return m_snapSettings; }
    void setSnapSettings(const Snapper::Settings& settings) { m_snapSettings = settings; }
    // Snap distance for the gesture's device in scene units.
    [[nodiscard]] double sceneSnapDistance(const PointerGesture& gesture) const;

private:
    Project& m_project;
    QUndoStack& m_undoStack;
    SelectionModel& m_selection;
    const InputSettings& m_input;
    ICanvasView* m_canvas = nullptr;
    ScreenId m_screenId;
    std::function<void()> m_selectToolRequest;
    std::function<void(QPointF)> m_contextMenuRequest;
    Snapper::Settings m_snapSettings;
    bool m_snapEnabled = true;
};

} // namespace hmi
