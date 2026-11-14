#pragma once

#include "core/model/Ids.h"
#include "editor/tools/ITool.h"

namespace hmi
{

class ToolContext;

// Places a new object of one type: a tap places it at its default size
// centred on the tap, a drag places it filling the dragged rectangle. Then
// the new object is selected and the select tool returns. Works the same with
// mouse, touch and pen.
class CreateTool : public ITool
{
public:
    // A drag smaller than this (view pixels) counts as a tap.
    static constexpr double kMinimumDragSize = 4.0;

    explicit CreateTool(ToolContext& context);

    void setType(ObjectTypeId type) noexcept { m_type = type; }
    [[nodiscard]] ObjectTypeId type() const noexcept { return m_type; }

    void deactivate() override;
    void handleGesture(const PointerGesture& gesture) override;
    bool keyPress(int key, Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] Qt::CursorShape cursor() const override { return Qt::CrossCursor; }

private:
    void place(const PointerGesture& gesture);

    ToolContext& m_context;
    ObjectTypeId m_type;
    bool m_dragging = false;
};

} // namespace hmi
