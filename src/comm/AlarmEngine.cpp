#include "comm/AlarmEngine.h"

#include "comm/TagEngine.h"

#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

namespace hmi
{

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
}

void AlarmEngine::start(const AlarmConfiguration& configuration)
{
    stop();
    m_configuration = configuration;
    m_states.assign(m_configuration.alarms.size(), State{});
    m_running = true;
    evaluate({}); // every alarm, with the values the tags start with
}

void AlarmEngine::stop()
{
    if (!m_running && m_pending.empty() && m_log.empty())
        return;
    m_running = false;
    m_states.clear();
    m_pending.clear();
    m_log.clear();
    Q_EMIT changed();
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

const Alarm* AlarmEngine::definition(int id) const
{
    return m_configuration.findAlarm(id);
}

int AlarmEngine::unacknowledgedCount() const
{
    return int(std::ranges::count_if(m_pending,
                                     [this](const PendingAlarm& alarm)
                                     {
                                         const AlarmClass* c = alarmClass(alarm.alarmClass);
                                         const bool needsAcknowledgment =
                                             c && c->stateMachine !=
                                                      AlarmStateMachine::WithoutAcknowledgment;
                                         return needsAcknowledgment &&
                                                (alarm.status == AlarmStatus::Incoming ||
                                                 alarm.status == AlarmStatus::IncomingOutgoing);
                                     }));
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

QString AlarmEngine::textOf(const Alarm& alarm) const
{
    // {Tag} becomes the tag's value at the time of the event.
    static const QRegularExpression placeholder(QStringLiteral("\\{([^{}]+)\\}"));
    QString text = alarm.text;
    QRegularExpressionMatchIterator it = placeholder.globalMatch(alarm.text);
    std::vector<std::pair<QString, QString>> values;
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1).trimmed();
        if (!m_tags.definition(name))
            continue;
        const TagValue value = m_tags.value(name);
        QString shown = TagValues::toText(value);
        if (const auto* number = std::get_if<double>(&value))
            shown = QLocale().toString(*number, 'f', 1);
        values.emplace_back(match.captured(0), shown);
    }
    for (const auto& [key, shown] : values)
        text.replace(key, shown);
    return text.isEmpty() ? alarm.name : text;
}

void AlarmEngine::evaluate(const QStringList& tags)
{
    if (!m_running)
        return;
    bool any = false;
    for (std::size_t i = 0; i < m_configuration.alarms.size(); ++i)
    {
        const Alarm& alarm = m_configuration.alarms[i];
        if (!tags.isEmpty() && !tags.contains(alarm.tag, Qt::CaseInsensitive))
            continue;
        const auto number = TagValues::toNumber(m_tags.value(alarm.tag));
        // A tag without a value (missing, not read yet) raises nothing.
        const bool active = number && alarm.isActive(*number);
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
    record(alarm, state);
}

void AlarmEngine::record(const Alarm& alarm, const State& state)
{
    const AlarmClass* c = alarmClass(alarm.alarmClass);
    const AlarmStateMachine machine = c ? c->stateMachine : AlarmStateMachine::SimpleAcknowledgment;
    const AlarmStatus status =
        statusOf(machine, state.active, state.acknowledged, state.acknowledgedWhileActive);
    const QDateTime now = QDateTime::currentDateTime();
    const QString text = textOf(alarm);
    m_log.insert(m_log.begin(), LoggedAlarm{alarm.id, text, alarm.alarmClass, status, now});
    if (m_log.size() > std::size_t(kLogSize))
        m_log.resize(std::size_t(kLogSize));

    std::erase_if(m_pending, [&alarm](const PendingAlarm& p) { return p.id == alarm.id; });
    if (status == AlarmStatus::Normal)
    {
        if (const auto index = std::ranges::find(m_configuration.alarms, alarm.id, &Alarm::id);
            index != m_configuration.alarms.end())
        {
            State& s = m_states[std::size_t(index - m_configuration.alarms.begin())];
            s.pending = false;
        }
        return;
    }
    PendingAlarm entry{alarm.id,       text,         alarm.alarmClass,
                       alarm.infoText, alarm.origin, c ? c->priority : 0,
                       status,         state.raised, now};
    // The newest first.
    const auto at = std::ranges::find_if(m_pending, [&entry](const PendingAlarm& p)
                                         { return p.raised <= entry.raised; });
    m_pending.insert(at, std::move(entry));
}

bool AlarmEngine::acknowledge(int id)
{
    const auto index = std::ranges::find(m_configuration.alarms, id, &Alarm::id);
    if (!m_running || index == m_configuration.alarms.end())
        return false;
    State& state = m_states[std::size_t(index - m_configuration.alarms.begin())];
    const AlarmClass* c = alarmClass(index->alarmClass);
    if (!state.pending || state.acknowledged || !c ||
        c->stateMachine == AlarmStateMachine::WithoutAcknowledgment)
        return false;
    state.acknowledged = true;
    state.acknowledgedWhileActive = state.active;
    record(*index, state);
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
    const auto index = std::ranges::find(m_configuration.alarms, id, &Alarm::id);
    if (!m_running || index == m_configuration.alarms.end())
        return false;
    State& state = m_states[std::size_t(index - m_configuration.alarms.begin())];
    const AlarmClass* c = alarmClass(index->alarmClass);
    if (!state.pending || !state.acknowledged || state.active || !c ||
        c->stateMachine != AlarmStateMachine::AcknowledgmentAndConfirmation)
        return false;
    // Confirmed: normal, no longer pending.
    state.pending = false;
    std::erase_if(m_pending, [id](const PendingAlarm& p) { return p.id == id; });
    m_log.insert(m_log.begin(), LoggedAlarm{id, textOf(*index), index->alarmClass,
                                            AlarmStatus::Normal, QDateTime::currentDateTime()});
    if (m_log.size() > std::size_t(kLogSize))
        m_log.resize(std::size_t(kLogSize));
    Q_EMIT changed();
    return true;
}

} // namespace hmi
