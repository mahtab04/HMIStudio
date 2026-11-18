#pragma once

#include "core/alarms/Alarm.h"
#include "core/model/Ids.h"

#include <QString>

#include <array>
#include <vector>

namespace hmi
{

// The lists an alarm control shows, as WinCC Unified's toolbar switches
// them: "Show active alarms", "Show logged alarms", "Alarm statistics -
// view", "Show defined alarms".
enum class AlarmList
{
    Active,
    Logged,
    Statistics,
    Defined,
};

// One line of an alarm control, ready to draw.
struct AlarmRow
{
    // The statistics columns, in this order, already formatted.
    enum StatisticsColumn
    {
        Frequency,
        SumActiveActive,
        SumActiveInactive,
        SumActiveAcknowledged,
        AverageActiveActive,
        AverageActiveInactive,
        AverageActiveAcknowledged,
        StatisticsColumnCount,
    };

    int id = 0;
    QString date;
    QString time;
    QString alarmClass;
    QString text;
    QString status;
    QString infoText;
    QString origin;
    int priority = 0;
    AlarmColors colors;
    // Defined alarms: the operator state of the alarm.
    bool enabled = true;
    bool shelved = false;
    // Statistics lines.
    std::array<QString, StatisticsColumnCount> statistics{};
};

// What an alarm control shows and which line is selected, kept per control
// while the runtime runs (the toolbar buttons change it).
struct AlarmViewState
{
    AlarmList list = AlarmList::Active;
    bool showCurrent = true; // "Alarms - show current": the newest is selected
    int selected = 0;        // row index while not showing current
    int firstRow = 0;        // the first visible row
    friend bool operator==(const AlarmViewState&, const AlarmViewState&) = default;
};

// The alarms of a running runtime, for alarm controls: their lines, their
// acknowledgment and their view state. Implemented next to the alarm
// engine; objects reach it through PaintContext and their runtime input.
class IAlarmSource
{
public:
    IAlarmSource() = default;
    virtual ~IAlarmSource() = default;
    IAlarmSource(const IAlarmSource&) = delete;
    IAlarmSource& operator=(const IAlarmSource&) = delete;
    IAlarmSource(IAlarmSource&&) = delete;
    IAlarmSource& operator=(IAlarmSource&&) = delete;

    // The lines of a list: active alarms and the log the newest first,
    // statistics and defined alarms by number.
    [[nodiscard]] virtual std::vector<AlarmRow> rows(AlarmList list) const = 0;
    // Pending (unshelved) alarms and those still to acknowledge.
    [[nodiscard]] virtual int pendingCount() const = 0;
    [[nodiscard]] virtual int unacknowledgedCount() const = 0;
    // Flashing colours show while this is true; it alternates.
    [[nodiscard]] virtual bool flashOn() const = 0;

    [[nodiscard]] virtual AlarmViewState viewState(ObjectId control) const = 0;
    virtual void setViewState(ObjectId control, const AlarmViewState& state) = 0;

    virtual bool acknowledge(int id) = 0;
    virtual int acknowledge(const std::vector<int>& ids) = 0;
    virtual bool confirm(int id) = 0;
    virtual bool disable(int id) = 0;
    virtual bool enable(int id) = 0;
    virtual bool shelve(int id) = 0;
    virtual bool unshelve(int id) = 0;
    // Writes a list to a CSV file; returns its path, or empty on failure.
    virtual QString exportList(AlarmList list) = 0;
};

} // namespace hmi
