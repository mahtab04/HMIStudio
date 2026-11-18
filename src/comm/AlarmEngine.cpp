#include "comm/AlarmEngine.h"

#include "comm/TagEngine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <map>
#include <optional>

namespace hmi
{

namespace
{

constexpr const char* kEventKeys[] = {"came", "went", "acknowledged", "confirmed"};
constexpr int kLogFormatVersion = 1;

std::optional<AlarmEvent> eventFromKey(const QString& key)
{
    for (int i = 0; i < 4; ++i)
    {
        if (key == QLatin1StringView(kEventKeys[i]))
            return AlarmEvent(i);
    }
    return std::nullopt;
}

double averageOf(double sum, int count)
{
    return count > 0 ? sum / count : 0.0;
}

} // namespace

double AlarmStatistics::averageActiveActive() const
{
    return averageOf(sumActiveActive, countActiveActive);
}

double AlarmStatistics::averageActiveInactive() const
{
    return averageOf(sumActiveInactive, countActiveInactive);
}

double AlarmStatistics::averageActiveAcknowledged() const
{
    return averageOf(sumActiveAcknowledged, countActiveAcknowledged);
}

AlarmEngine::AlarmEngine(TagEngine& tags, QObject* parent)
    : QObject(parent)
    , m_tags(tags)
{
    connect(&m_tags, &TagEngine::valuesChanged, this, &AlarmEngine::evaluate);
    connect(&m_tags, &TagEngine::runningChanged, this,
            [this]
            {
                if (!m_tags.isRunning())
                    stop();
            });
    m_save.setSingleShot(true);
    m_save.setInterval(kSaveDelayMs);
    connect(&m_save, &QTimer::timeout, this, &AlarmEngine::saveLog);
    m_shelving.setInterval(1000);
    connect(&m_shelving, &QTimer::timeout, this, &AlarmEngine::unshelveExpired);
}

AlarmEngine::~AlarmEngine()
{
    saveLog();
}

void AlarmEngine::setLogFile(const QString& path)
{
    if (path == m_logFile)
        return;
    saveLog();
    m_logFile = path;
}

void AlarmEngine::start(const AlarmConfiguration& configuration)
{
    stop();
    m_configuration = configuration;
    m_states.assign(m_configuration.alarms.size(), State{});
    loadLog();
    m_running = true;
    evaluate({}); // every alarm, with the values the tags start with
}

void AlarmEngine::stop()
{
    m_shelving.stop();
    if (!m_running && m_pending.empty() && m_log.empty())
        return;
    saveLog();
    m_running = false;
    m_states.clear();
    m_pending.clear();
    m_log.clear();
    Q_EMIT changed();
}

int AlarmEngine::indexOf(int id) const
{
    const auto it = std::ranges::find(m_configuration.alarms, id, &Alarm::id);
    return it == m_configuration.alarms.end() ? -1 : int(it - m_configuration.alarms.begin());
}

const PendingAlarm* AlarmEngine::find(int id) const
{
    const auto it = std::ranges::find(m_pending, id, &PendingAlarm::id);
    return it == m_pending.end() ? nullptr : &*it;
}

const AlarmClass* AlarmEngine::alarmClass(const QString& name) const
{
    return m_configuration.findClass(name);
}

QString AlarmEngine::statusText(AlarmStatus status) const
{
    return m_configuration.settings.statusText(status);
}

bool AlarmEngine::needsAcknowledgment(const PendingAlarm& alarm) const
{
    const AlarmClass* c = alarmClass(alarm.alarmClass);
    return !alarm.shelved && c && c->stateMachine != AlarmStateMachine::WithoutAcknowledgment &&
           (alarm.status == AlarmStatus::Incoming || alarm.status == AlarmStatus::IncomingOutgoing);
}

int AlarmEngine::unacknowledgedCount() const
{
    return int(std::ranges::count_if(m_pending, [this](const PendingAlarm& alarm)
                                     { return needsAcknowledgment(alarm); }));
}

std::vector<DefinedAlarm> AlarmEngine::defined() const
{
    std::vector<DefinedAlarm> result;
    for (std::size_t i = 0; i < m_configuration.alarms.size() && i < m_states.size(); ++i)
    {
        const Alarm& alarm = m_configuration.alarms[i];
        const State& state = m_states[i];
        const AlarmClass* c = alarmClass(alarm.alarmClass);
        const PendingAlarm* pending = find(alarm.id);
        result.push_back({alarm.id, pending ? pending->text : textOf(alarm), alarm.alarmClass,
                          alarm.infoText, alarm.origin, c ? c->priority : 0,
                          pending ? pending->status : AlarmStatus::Normal, state.enabled,
                          state.shelved, state.shelvedUntil});
    }
    std::ranges::sort(result, {}, &DefinedAlarm::id);
    return result;
}

AlarmStatus AlarmEngine::statusOf(AlarmStateMachine machine, bool active, bool acknowledged,
                                  bool acknowledgedWhileActive)
{
    switch (machine)
    {
    case AlarmStateMachine::WithoutAcknowledgment:
        return active ? AlarmStatus::Incoming : AlarmStatus::Normal;
    case AlarmStateMachine::SimpleAcknowledgment:
        if (active)
            return acknowledged ? AlarmStatus::IncomingAcknowledged : AlarmStatus::Incoming;
        return acknowledged ? AlarmStatus::Normal : AlarmStatus::IncomingOutgoing;
    case AlarmStateMachine::AcknowledgmentAndConfirmation:
        if (active)
            return acknowledged ? AlarmStatus::IncomingAcknowledged : AlarmStatus::Incoming;
        if (!acknowledged)
            return AlarmStatus::IncomingOutgoing;
        return acknowledgedWhileActive ? AlarmStatus::IncomingAcknowledgedOutgoing
                                       : AlarmStatus::IncomingOutgoingAcknowledged;
    }
    return AlarmStatus::Normal;
}

std::vector<AlarmStatistics> AlarmEngine::statisticsOf(const std::vector<LoggedAlarm>& log)
{
    struct Tracking
    {
        AlarmStatistics line;
        QDateTime came;
        bool waitingToGo = false;
        bool waitingForAcknowledgment = false;
    };
    std::map<int, Tracking> byId;
    const auto seconds = [](const QDateTime& from, const QDateTime& to)
    {
        return double(from.msecsTo(to)) / 1000.0;
    };
    // Oldest first. Going and acknowledgment count only after a coming.
    for (auto it = log.rbegin(); it != log.rend(); ++it)
    {
        Tracking& t = byId[it->id];
        t.line.id = it->id;
        t.line.text = it->text;
        switch (it->event)
        {
        case AlarmEvent::Came:
            ++t.line.frequency;
            if (t.came.isValid())
            {
                t.line.sumActiveActive += seconds(t.came, it->time);
                ++t.line.countActiveActive;
            }
            t.came = it->time;
            t.waitingToGo = true;
            t.waitingForAcknowledgment = true;
            break;
        case AlarmEvent::Went:
            if (t.came.isValid() && t.waitingToGo)
            {
                t.line.sumActiveInactive += seconds(t.came, it->time);
                ++t.line.countActiveInactive;
                t.waitingToGo = false;
            }
            break;
        case AlarmEvent::Acknowledged:
            if (t.came.isValid() && t.waitingForAcknowledgment)
            {
                t.line.sumActiveAcknowledged += seconds(t.came, it->time);
                ++t.line.countActiveAcknowledged;
                t.waitingForAcknowledgment = false;
            }
            break;
        case AlarmEvent::Confirmed:
            break;
        }
    }
    std::vector<AlarmStatistics> result;
    for (auto& [id, t] : byId)
    {
        // Alarms that never came in the log are left out.
        if (t.line.frequency > 0)
            result.push_back(std::move(t.line));
    }
    return result;
}

QString AlarmEngine::textOf(const Alarm& alarm) const
{
    return Alarms::formatText(alarm,
                              [this](const QString& name) -> std::optional<TagValue>
                              {
                                  if (!m_tags.definition(name))
                                      return std::nullopt;
                                  return m_tags.value(name);
                              });
}

bool AlarmEngine::conditionOf(const Alarm& alarm) const
{
    const auto number = TagValues::toNumber(m_tags.value(alarm.tag));
    // A tag without a value (missing, not read yet) raises nothing.
    return number && alarm.isActive(*number);
}

void AlarmEngine::evaluate(const QStringList& tags)
{
    if (!m_running)
        return;
    bool any = false;
    for (std::size_t i = 0; i < m_configuration.alarms.size(); ++i)
    {
        const Alarm& alarm = m_configuration.alarms[i];
        if (!m_states[i].enabled)
            continue;
        if (!tags.isEmpty() && !tags.contains(alarm.tag, Qt::CaseInsensitive))
            continue;
        const bool active = conditionOf(alarm);
        if (active == m_states[i].active)
            continue;
        update(alarm, m_states[i], active);
        any = true;
    }
    if (any)
        Q_EMIT changed();
}

void AlarmEngine::update(const Alarm& alarm, State& state, bool active)
{
    state.active = active;
    if (active)
    {
        // Comes, or comes again while it is still pending: a new incoming.
        state.pending = true;
        state.acknowledged = false;
        state.acknowledgedWhileActive = false;
        state.raised = QDateTime::currentDateTime();
    }
    else if (!state.pending)
    {
        return;
    }
    record(alarm, state, active ? AlarmEvent::Came : AlarmEvent::Went);
}

void AlarmEngine::appendLog(LoggedAlarm entry)
{
    m_log.insert(m_log.begin(), std::move(entry));
    if (m_log.size() > std::size_t(kLogSize))
        m_log.resize(std::size_t(kLogSize));
    m_logDirty = true;
    if (!m_logFile.isEmpty())
        m_save.start();
}

void AlarmEngine::record(const Alarm& alarm, const State& state, AlarmEvent event)
{
    const AlarmClass* c = alarmClass(alarm.alarmClass);
    const AlarmStateMachine machine = c ? c->stateMachine : AlarmStateMachine::SimpleAcknowledgment;
    const AlarmStatus status =
        statusOf(machine, state.active, state.acknowledged, state.acknowledgedWhileActive);
    const QDateTime now = QDateTime::currentDateTime();
    const QString text = textOf(alarm);
    appendLog({alarm.id, text, alarm.alarmClass, status, now, event});

    std::erase_if(m_pending, [&alarm](const PendingAlarm& p) { return p.id == alarm.id; });
    if (status == AlarmStatus::Normal)
    {
        if (const int index = indexOf(alarm.id); index >= 0)
            m_states[std::size_t(index)].pending = false;
        return;
    }
    PendingAlarm entry{alarm.id,       text,         alarm.alarmClass,
                       alarm.infoText, alarm.origin, c ? c->priority : 0,
                       status,         state.raised, now,
                       state.shelved};
    // The newest first.
    const auto at = std::ranges::find_if(m_pending, [&entry](const PendingAlarm& p)
                                         { return p.raised <= entry.raised; });
    m_pending.insert(at, std::move(entry));
}

bool AlarmEngine::acknowledge(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0)
        return false;
    const Alarm& alarm = m_configuration.alarms[std::size_t(index)];
    State& state = m_states[std::size_t(index)];
    const AlarmClass* c = alarmClass(alarm.alarmClass);
    if (!state.pending || state.acknowledged || !c ||
        c->stateMachine == AlarmStateMachine::WithoutAcknowledgment)
        return false;
    state.acknowledged = true;
    state.acknowledgedWhileActive = state.active;
    record(alarm, state, AlarmEvent::Acknowledged);
    Q_EMIT changed();
    return true;
}

int AlarmEngine::acknowledge(const std::vector<int>& ids)
{
    QSignalBlocker block(this);
    int done = 0;
    for (const int id : ids)
        done += acknowledge(id) ? 1 : 0;
    block.unblock();
    if (done > 0)
        Q_EMIT changed();
    return done;
}

bool AlarmEngine::confirm(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0)
        return false;
    const Alarm& alarm = m_configuration.alarms[std::size_t(index)];
    State& state = m_states[std::size_t(index)];
    const AlarmClass* c = alarmClass(alarm.alarmClass);
    if (!state.pending || !state.acknowledged || state.active || !c ||
        c->stateMachine != AlarmStateMachine::AcknowledgmentAndConfirmation)
        return false;
    // Confirmed: normal, no longer pending.
    state.pending = false;
    std::erase_if(m_pending, [id](const PendingAlarm& p) { return p.id == id; });
    appendLog({id, textOf(alarm), alarm.alarmClass, AlarmStatus::Normal,
               QDateTime::currentDateTime(), AlarmEvent::Confirmed});
    Q_EMIT changed();
    return true;
}

bool AlarmEngine::disable(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0 || !m_states[std::size_t(index)].enabled)
        return false;
    // Gone from the lists and no longer logged; nothing is recorded.
    m_states[std::size_t(index)] = State{};
    m_states[std::size_t(index)].enabled = false;
    std::erase_if(m_pending, [id](const PendingAlarm& p) { return p.id == id; });
    Q_EMIT changed();
    return true;
}

bool AlarmEngine::enable(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0 || m_states[std::size_t(index)].enabled)
        return false;
    State& state = m_states[std::size_t(index)];
    state.enabled = true;
    // Checked again: it comes if its cause still exists.
    const Alarm& alarm = m_configuration.alarms[std::size_t(index)];
    if (conditionOf(alarm))
        update(alarm, state, true);
    Q_EMIT changed();
    return true;
}

bool AlarmEngine::shelve(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0)
        return false;
    State& state = m_states[std::size_t(index)];
    if (!state.enabled || state.shelved)
        return false;
    state.shelved = true;
    const int timeout = m_configuration.settings.shelvingTimeout;
    state.shelvedUntil = timeout > 0 ? QDateTime::currentDateTime().addSecs(timeout) : QDateTime();
    for (PendingAlarm& p : m_pending)
    {
        if (p.id == id)
            p.shelved = true;
    }
    if (timeout > 0 && !m_shelving.isActive())
        m_shelving.start();
    Q_EMIT changed();
    return true;
}

bool AlarmEngine::unshelve(int id)
{
    const int index = indexOf(id);
    if (!m_running || index < 0 || !m_states[std::size_t(index)].shelved)
        return false;
    State& state = m_states[std::size_t(index)];
    state.shelved = false;
    state.shelvedUntil = {};
    for (PendingAlarm& p : m_pending)
    {
        if (p.id == id)
            p.shelved = false;
    }
    if (std::ranges::none_of(m_states, &State::shelved))
        m_shelving.stop();
    Q_EMIT changed();
    return true;
}

void AlarmEngine::unshelveExpired()
{
    const QDateTime now = QDateTime::currentDateTime();
    std::vector<int> expired;
    for (std::size_t i = 0; i < m_states.size(); ++i)
    {
        const State& state = m_states[i];
        if (state.shelved && state.shelvedUntil.isValid() && state.shelvedUntil <= now)
            expired.push_back(m_configuration.alarms[i].id);
    }
    for (const int id : expired)
        unshelve(id);
}

void AlarmEngine::loadLog()
{
    m_log.clear();
    m_logDirty = false;
    if (m_logFile.isEmpty())
        return;
    QFile file(m_logFile);
    if (!file.open(QIODevice::ReadOnly))
        return; // no log yet
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    if (json.value(QStringLiteral("formatVersion")).toInt() > kLogFormatVersion)
        return; // from a newer version: left alone, not overwritten below
    for (const QJsonValue& value : json.value(QStringLiteral("entries")).toArray())
    {
        const QJsonObject e = value.toObject();
        const auto status = Alarms::statusFromKey(e.value(QStringLiteral("status")).toString());
        const auto event = eventFromKey(e.value(QStringLiteral("event")).toString());
        const QDateTime time =
            QDateTime::fromString(e.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
        if (!status || !event || !time.isValid())
            continue;
        m_log.push_back(
            {e.value(QStringLiteral("id")).toInt(), e.value(QStringLiteral("text")).toString(),
             e.value(QStringLiteral("class")).toString(), *status, time.toLocalTime(), *event});
        if (m_log.size() >= std::size_t(kLogSize))
            break;
    }
}

void AlarmEngine::saveLog()
{
    m_save.stop();
    if (m_logFile.isEmpty() || !m_logDirty)
        return;
    QJsonArray entries;
    for (const LoggedAlarm& entry : m_log)
        entries.append(
            QJsonObject{{QStringLiteral("id"), entry.id},
                        {QStringLiteral("text"), entry.text},
                        {QStringLiteral("class"), entry.alarmClass},
                        {QStringLiteral("status"), Alarms::statusKey(entry.status)},
                        {QStringLiteral("event"), QLatin1StringView(kEventKeys[int(entry.event)])},
                        {QStringLiteral("time"), entry.time.toUTC().toString(Qt::ISODateWithMs)}});
    QDir().mkpath(QFileInfo(m_logFile).absolutePath());
    QSaveFile file(m_logFile);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("formatVersion"), kLogFormatVersion},
                                         {QStringLiteral("entries"), entries}})
                   .toJson(QJsonDocument::Indented));
    if (file.commit())
        m_logDirty = false;
}

} // namespace hmi
