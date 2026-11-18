#pragma once

#include "core/alarms/Alarm.h"
#include "core/model/Ids.h"

#include <QString>

#include <vector>

namespace hmi
{

// One line of an alarm control, ready to draw.
struct AlarmRow
{
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
};

// What an alarm control shows and which line is selected, kept per control
// while the runtime runs (the toolbar buttons change it).
struct AlarmViewState
{
    bool logged = false;     // "Show logged alarms" instead of active ones
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

    // Active alarms (the newest first), or the alarm log.
    [[nodiscard]] virtual std::vector<AlarmRow> rows(bool logged) const = 0;
    // Pending alarms and those still to acknowledge.
    [[nodiscard]] virtual int pendingCount() const = 0;
    [[nodiscard]] virtual int unacknowledgedCount() const = 0;
    // Flashing colours show while this is true; it alternates.
    [[nodiscard]] virtual bool flashOn() const = 0;

    [[nodiscard]] virtual AlarmViewState viewState(ObjectId control) const = 0;
    virtual void setViewState(ObjectId control, const AlarmViewState& state) = 0;

    virtual bool acknowledge(int id) = 0;
    virtual int acknowledge(const std::vector<int>& ids) = 0;
    virtual bool confirm(int id) = 0;
};

} // namespace hmi
