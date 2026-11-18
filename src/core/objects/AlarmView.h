#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// The alarm control of WinCC Unified: at runtime it lists the active alarms
// (or the alarm log) in their class colours, newest first, with a toolbar to
// switch views, move the selection and acknowledge or confirm. In the editor
// it shows its layout: toolbar, column headers and empty rows.
class AlarmView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"alarmView"};

    explicit AlarmView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;
    [[nodiscard]] RuntimeBehavior runtimeBehavior() const override;
    [[nodiscard]] bool usesAlarms() const override { return true; }
    bool operateAt(QPointF point, IAlarmSource* alarms) const override;

    // The toolbar buttons, as in the WinCC Unified alarm control.
    enum ToolbarButton
    {
        ShowActive, // "Show active alarms"
        ShowLogged, // "Show logged alarms"
        FirstLine,
        PreviousLine,
        NextLine,
        LastLine,
        Acknowledge,        // "Single acknowledgment"
        AcknowledgeVisible, // "Acknowledge visible alarms"
        Confirm,            // "Single confirm"
        ShowCurrent,        // "Alarms - show current"
        ButtonCount,
    };
    // Where a toolbar button and a visible line are, in local coordinates.
    [[nodiscard]] QRectF buttonRect(ToolbarButton button) const;
    [[nodiscard]] QRectF lineRect(int visibleLine) const;

private:
    AlarmView(const AlarmView&) = default;
};

} // namespace hmi
