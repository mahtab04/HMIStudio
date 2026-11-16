#pragma once

#include "core/model/Ids.h"
#include "quick/input/PointerGesture.h"

#include <QObject>

#include <memory>

namespace hmi
{

class CreateTool;
class ITool;
class PanTool;
class PathTool;
class SelectTool;
class ToolContext;

// Holds the canvas tools and routes gestures and keys to the current one.
// Navigation (pinch, two-finger pan, wheel, middle button, Space+drag) is
// handled here for every tool, so it is always available.
class ToolController : public QObject
{
    Q_OBJECT

public:
    enum class Tool
    {
        Select,
        Pan,
        Create,
        Path, // placing a type point by point
    };

    // Wheel zoom per angle unit: one notch (120) zooms by about 20 %.
    static constexpr double kWheelZoomBase = 1.0015;
    static constexpr double kWheelPanFactor = 0.5;

    explicit ToolController(ToolContext& context, QObject* parent = nullptr);
    ~ToolController() override;
    ToolController(const ToolController&) = delete;
    ToolController& operator=(const ToolController&) = delete;
    ToolController(ToolController&&) = delete;
    ToolController& operator=(ToolController&&) = delete;

    [[nodiscard]] Tool tool() const noexcept { return m_tool; }
    void setTool(Tool tool);

    // Switches to the tool that places `type`: the create tool, or the path
    // tool for types placed point by point.
    void createObjects(ObjectTypeId type);
    [[nodiscard]] ObjectTypeId createType() const;

    void handleGesture(const PointerGesture& gesture);
    bool keyPress(int key, Qt::KeyboardModifiers modifiers);

    // Re-applies the current tool's cursor (after a canvas is attached).
    void applyCursor();
    // Pinch, wheel and two-finger pan: the view moves, nothing is edited.
    // Returns false for gestures that are not navigation.
    bool navigate(const PointerGesture& gesture);

Q_SIGNALS:
    void toolChanged();

private:
    [[nodiscard]] ITool& current() const;

    ToolContext& m_context;
    std::unique_ptr<SelectTool> m_select;
    std::unique_ptr<PanTool> m_pan;
    std::unique_ptr<CreateTool> m_create;
    std::unique_ptr<PathTool> m_path;
    Tool m_tool = Tool::Select;
};

} // namespace hmi
