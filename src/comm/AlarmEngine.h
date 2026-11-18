#pragma once

#include "core/alarms/Alarm.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimer>

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
    QDateTime raised;     // when it came
    QDateTime modified;   // its last status change
    bool shelved = false; // hidden from the active alarms until unshelved
};

// What happened to an alarm: what a log entry records.
enum class AlarmEvent
{
    Came,
    Went,
    Acknowledged,
    Confirmed,
};

// One entry of the alarm log: a status change of an alarm.
struct LoggedAlarm
{
    int id = 0;
    QString text;
    QString alarmClass;
    AlarmStatus status = AlarmStatus::Incoming;
    QDateTime time;
    AlarmEvent event = AlarmEvent::Came;
};

// One configured alarm with its operator state, for "Show defined alarms".
struct DefinedAlarm
{
    int id = 0;
    QString text;
    QString alarmClass;
    QString infoText;
    QString origin;
    int priority = 0;
    AlarmStatus status = AlarmStatus::Normal;
    bool enabled = true;
    bool shelved = false;
    QDateTime shelvedUntil; // invalid: until unshelved
};

// One line of the alarm statistics, from the log, as WinCC Unified's:
// how often an alarm came, and the time from coming to coming again
// (active-active), to going (active-inactive) and to its acknowledgment
// (active-acknowledged), in seconds.
struct AlarmStatistics
{
    int id = 0;
    QString text;
    int frequency = 0;
    double sumActiveActive = 0;
    double sumActiveInactive = 0;
    double sumActiveAcknowledged = 0;
    int countActiveActive = 0;
    int countActiveInactive = 0;
    int countActiveAcknowledged = 0;

    [[nodiscard]] double averageActiveActive() const;
    [[nodiscard]] double averageActiveInactive() const;
    [[nodiscard]] double averageActiveAcknowledged() const;
};

// Runs the project's alarms against the tag values while simulating (later
// the runtime): discrete alarms watch a bit of their trigger tag, analog
// alarms compare it with a limit. Statuses follow the alarm class's state
// machine, as WinCC Unified: Incoming, Incoming/Outgoing,
// Incoming/Acknowledged, ..., and an alarm is no longer pending once it
// reaches Normal. Every status change is logged (the newest kLogSize); with
// a log file the log is kept across runs.
//
// Operators may disable an alarm (it is no longer raised or logged; when
// enabled again it comes if its condition holds) or shelve it (it stays
// out of the active alarms and the alarm line until unshelved or the
// shelving timeout passes). Both end with the run.
class AlarmEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int kLogSize = 1000;
    static constexpr int kSaveDelayMs = 1000;

    explicit AlarmEngine(TagEngine& tags, QObject* parent = nullptr);
    ~AlarmEngine() override;
    AlarmEngine(const AlarmEngine&) = delete;
    AlarmEngine& operator=(const AlarmEngine&) = delete;
    AlarmEngine(AlarmEngine&&) = delete;
    AlarmEngine& operator=(AlarmEngine&&) = delete;

    // Where the log is kept between runs; empty keeps it for one run.
    void setLogFile(const QString& path);
    [[nodiscard]] const QString& logFile() const noexcept { return m_logFile; }

    // Starts watching the tags with these alarms; the tag engine runs.
    void start(const AlarmConfiguration& configuration);
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_running; }

    // Pending alarms, the newest first, shelved ones included.
    [[nodiscard]] const std::vector<PendingAlarm>& pending() const noexcept { return m_pending; }
    // Logged status changes, the newest first.
    [[nodiscard]] const std::vector<LoggedAlarm>& log() const noexcept { return m_log; }
    // Every configured alarm, by number.
    [[nodiscard]] std::vector<DefinedAlarm> defined() const;
    [[nodiscard]] std::vector<AlarmStatistics> statistics() const { return statisticsOf(m_log); }
    [[nodiscard]] const PendingAlarm* find(int id) const;
    [[nodiscard]] const AlarmClass* alarmClass(const QString& name) const;
    // The status text of the runtime settings.
    [[nodiscard]] QString statusText(AlarmStatus status) const;
    // Pending, unshelved alarms waiting for acknowledgment.
    [[nodiscard]] int unacknowledgedCount() const;
    [[nodiscard]] bool needsAcknowledgment(const PendingAlarm& alarm) const;

    // Single acknowledgment. False when the alarm is not pending or needs
    // none (or has it already).
    bool acknowledge(int id);
    // Group acknowledgment: the given alarms (the visible ones).
    int acknowledge(const std::vector<int>& ids);
    // Single confirm, for "Alarm with acknowledgment and confirmation"
    // alarms that were acknowledged and have gone.
    bool confirm(int id);

    bool disable(int id);
    bool enable(int id);
    bool shelve(int id);
    bool unshelve(int id);

    // The status of an alarm of a class with `machine`, by its condition and
    // acknowledgment; WinCC Unified's state tables.
    [[nodiscard]] static AlarmStatus statusOf(AlarmStateMachine machine, bool active,
                                              bool acknowledged, bool acknowledgedWhileActive);
    // The statistics of a log (the newest entry first).
    [[nodiscard]] static std::vector<AlarmStatistics>
    statisticsOf(const std::vector<LoggedAlarm>& log);

Q_SIGNALS:
    // Pending alarms, the log or an operator state changed.
    void changed();

private:
    struct State
    {
        bool active = false;
        bool pending = false;
        bool acknowledged = false;
        bool acknowledgedWhileActive = false;
        bool enabled = true;
        bool shelved = false;
        QDateTime raised;
        QDateTime shelvedUntil;
    };

    void evaluate(const QStringList& tags);
    [[nodiscard]] bool conditionOf(const Alarm& alarm) const;
    void update(const Alarm& alarm, State& state, bool active);
    void record(const Alarm& alarm, const State& state, AlarmEvent event);
    void appendLog(LoggedAlarm entry);
    void unshelveExpired();
    [[nodiscard]] QString textOf(const Alarm& alarm) const;
    // The index of an alarm in the configuration, or -1.
    [[nodiscard]] int indexOf(int id) const;
    void loadLog();
    void saveLog();

    TagEngine& m_tags;
    AlarmConfiguration m_configuration;
    std::vector<State> m_states; // per alarm of m_configuration
    std::vector<PendingAlarm> m_pending;
    std::vector<LoggedAlarm> m_log;
    bool m_running = false;
    QString m_logFile;
    bool m_logDirty = false;
    QTimer m_save;     // writes the log a moment after it changed
    QTimer m_shelving; // unshelves alarms whose shelving timed out
};

} // namespace hmi
