#pragma once

#include "core/alarms/Alarm.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>

#include <vector>

namespace hmi
{

class TagEngine;

// One pending alarm, as an alarm control shows it.
struct PendingAlarm
{
    int id = 0;
    QString text; // with tag values filled in
    QString alarmClass;
    QString infoText;
    QString origin;
    int priority = 0;
    AlarmStatus status = AlarmStatus::Incoming;
    QDateTime raised;   // when it came
    QDateTime modified; // its last status change
};

// One entry of the alarm log: a status change of an alarm.
struct LoggedAlarm
{
    int id = 0;
    QString text;
    QString alarmClass;
    AlarmStatus status = AlarmStatus::Incoming;
    QDateTime time;
};

// Runs the project's alarms against the tag values while simulating (later
// the runtime): discrete alarms watch a bit of their trigger tag, analog
// alarms compare it with a limit. Statuses follow the alarm class's state
// machine, as WinCC Unified: Incoming, Incoming/Outgoing,
// Incoming/Acknowledged, ..., and an alarm is no longer pending once it
// reaches Normal. Every status change is logged (the newest kLogSize).
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int kLogSize = 1000;

    explicit AlarmEngine(TagEngine& tags, QObject* parent = nullptr);

    // Starts watching the tags with these alarms; the tag engine runs.
    void start(const AlarmConfiguration& configuration);
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_running; }

    // Pending alarms, the newest first.
    [[nodiscard]] const std::vector<PendingAlarm>& pending() const noexcept { return m_pending; }
    // Logged status changes, the newest first.
    [[nodiscard]] const std::vector<LoggedAlarm>& log() const noexcept { return m_log; }
    [[nodiscard]] const PendingAlarm* find(int id) const;
    [[nodiscard]] const AlarmClass* alarmClass(const QString& name) const;
    // Pending alarms waiting for acknowledgment.
    [[nodiscard]] int unacknowledgedCount() const;

    // Single acknowledgment. False when the alarm is not pending or needs
    // none (or has it already).
    bool acknowledge(int id);
    // Group acknowledgment: the given alarms (the visible ones).
    int acknowledge(const std::vector<int>& ids);
    // Single confirm, for "Alarm with acknowledgment and confirmation"
    // alarms that were acknowledged and have gone.
    bool confirm(int id);

    // The status of an alarm of a class with `machine`, by its condition and
    // acknowledgment; WinCC Unified's state tables.
    [[nodiscard]] static AlarmStatus statusOf(AlarmStateMachine machine, bool active,
                                              bool acknowledged, bool acknowledgedWhileActive);

Q_SIGNALS:
    // Pending alarms or the log changed.
    void changed();

private:
    struct State
    {
        bool active = false;
        bool pending = false;
        bool acknowledged = false;
        bool acknowledgedWhileActive = false;
        QDateTime raised;
    };

    void evaluate(const QStringList& tags);
    void update(const Alarm& alarm, State& state, bool active);
    void record(const Alarm& alarm, const State& state);
    [[nodiscard]] QString textOf(const Alarm& alarm) const;
    [[nodiscard]] const Alarm* definition(int id) const;

    TagEngine& m_tags;
    AlarmConfiguration m_configuration;
    std::vector<State> m_states; // per alarm of m_configuration
    std::vector<PendingAlarm> m_pending;
    std::vector<LoggedAlarm> m_log;
    bool m_running = false;
};

} // namespace hmi
