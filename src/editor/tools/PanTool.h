#pragma once

#include "editor/tools/ITool.h"

namespace hmi
{

class ToolContext;

// Pans the view with a one-finger or left-button drag. Pinch, wheel, middle
// button and Space+drag navigate with every tool (ToolController); this tool
// makes plain dragging pan too.
class PanTool : public ITool
{
public:
    explicit PanTool(ToolContext& context);

    void handleGesture(const PointerGesture& gesture) override;
    [[nodiscard]] Qt::CursorShape cursor() const override { return Qt::OpenHandCursor; }

private:
    ToolContext& m_context;
};

} // namespace hmi
