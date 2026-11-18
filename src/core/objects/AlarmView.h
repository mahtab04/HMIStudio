#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// The alarm control of WinCC Unified: at runtime it lists the active alarms
// in their class colours, newest first, or the alarm log, the alarm
// statistics or the defined alarms, with a toolbar to switch lists, move
// the selection, acknowledge or confirm, shelve, disable and export. In the
// editor it shows its layout: toolbar, column headers and empty rows.
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
        ShowActive,     // "Show active alarms"
        ShowLogged,     // "Show logged alarms"
        ShowStatistics, // "Alarm statistics - view"
        ShowDefined,    // "Show defined alarms"
        FirstLine,
        PreviousLine,
        NextLine,
        LastLine,
        Acknowledge,        // "Single acknowledgment"
        AcknowledgeVisible, // "Acknowledge visible alarms"
        Confirm,            // "Single confirm"
        Shelve,             // "Shelve alarm": active or defined alarms
        Unshelve,           // "Unshelve alarm": defined alarms
        Disable,            // "Disable alarm": defined alarms
        Enable,             // "Enable alarm": defined alarms
        Export,             // "Export": the list shown, to CSV
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
