#include "comm/RuntimeAlarms.h"

#include <QLocale>

#include <algorithm>

namespace hmi
{

RuntimeAlarms::RuntimeAlarms(AlarmEngine& engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
    m_flash.setInterval(kFlashMs);
    connect(&m_flash, &QTimer::timeout, this,
            [this]
            {
                m_flashOn = !m_flashOn;
                Q_EMIT changed();
            });
    connect(&m_engine, &AlarmEngine::changed, this, &RuntimeAlarms::onAlarmsChanged);
}

void RuntimeAlarms::onAlarmsChanged()
{
    // Flash only while an alarm in a flashing status is pending.
    const bool flashing = std::ranges::any_of(m_engine.pending(),
                                              [this](const PendingAlarm& alarm)
                                              {
                                                  const AlarmClass* c =
                                                      m_engine.alarmClass(alarm.alarmClass);
                                                  return c && c->colorsFor(alarm.status).flashing;
                                              });
    if (flashing && !m_flash.isActive())
        m_flash.start();
    else if (!flashing)
    {
        m_flash.stop();
        m_flashOn = true;
    }
    // Selections past the end move back to the last line.
    for (auto it = m_views.begin(); it != m_views.end(); ++it)
    {
        const int count = int(rows(it->logged).size());
        it->selected = std::clamp(it->selected, 0, std::max(0, count - 1));
        it->firstRow = std::clamp(it->firstRow, 0, std::max(0, count - 1));
    }
    Q_EMIT changed();
}

std::vector<AlarmRow> RuntimeAlarms::rows(bool logged) const
{
    std::vector<AlarmRow> result;
    const QLocale locale;
    const auto colorsOf = [this](const QString& name, AlarmStatus status)
    {
        const AlarmClass* c = m_engine.alarmClass(name);
        return c ? c->colorsFor(status) : AlarmColors{};
    };
    if (logged)
    {
        for (const LoggedAlarm& entry : m_engine.log())
        {
            const AlarmClass* c = m_engine.alarmClass(entry.alarmClass);
            // A log line in the colours of its status; normal ones plain.
            result.push_back(
                {entry.id,
                 locale.toString(entry.time.date(), QLocale::ShortFormat),
                 locale.toString(entry.time.time(), QStringLiteral("HH:mm:ss")),
                 entry.alarmClass,
                 entry.text,
                 Alarms::statusName(entry.status),
                 {},
                 {},
                 c ? c->priority : 0,
                 entry.status == AlarmStatus::Normal
                     ? AlarmColors{}
                     : AlarmColors{colorsOf(entry.alarmClass, entry.status).text,
                                   colorsOf(entry.alarmClass, entry.status).background, false}});
        }
        return result;
    }
    for (const PendingAlarm& alarm : m_engine.pending())
        result.push_back({alarm.id, locale.toString(alarm.raised.date(), QLocale::ShortFormat),
                          locale.toString(alarm.raised.time(), QStringLiteral("HH:mm:ss")),
                          alarm.alarmClass, alarm.text, Alarms::statusName(alarm.status),
                          alarm.infoText, alarm.origin, alarm.priority,
                          colorsOf(alarm.alarmClass, alarm.status)});
    return result;
}

int RuntimeAlarms::pendingCount() const
{
    return int(m_engine.pending().size());
}

int RuntimeAlarms::unacknowledgedCount() const
{
    return m_engine.unacknowledgedCount();
}

AlarmViewState RuntimeAlarms::viewState(ObjectId control) const
{
    return m_views.value(control);
}

void RuntimeAlarms::setViewState(ObjectId control, const AlarmViewState& state)
{
    if (m_views.value(control) == state && m_views.contains(control))
        return;
    m_views.insert(control, state);
    Q_EMIT changed();
}

bool RuntimeAlarms::acknowledge(int id)
{
    return m_engine.acknowledge(id);
}

int RuntimeAlarms::acknowledge(const std::vector<int>& ids)
{
    return m_engine.acknowledge(ids);
}

bool RuntimeAlarms::confirm(int id)
{
    return m_engine.confirm(id);
}

void RuntimeAlarms::reset()
{
    m_views.clear();
    Q_EMIT changed();
}

} // namespace hmi
