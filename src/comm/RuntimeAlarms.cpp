#include "comm/RuntimeAlarms.h"

#include <QDir>
#include <QLocale>
#include <QSaveFile>

#include <algorithm>

namespace hmi
{

namespace
{

QString csvField(const QString& text)
{
    if (!text.contains(u',') && !text.contains(u'"') && !text.contains(u'\n'))
        return text;
    QString quoted = text;
    quoted.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return u'"' + quoted + u'"';
}

QString csvLine(const QStringList& fields)
{
    QStringList quoted;
    for (const QString& field : fields)
        quoted << csvField(field);
    return quoted.join(u',') + QStringLiteral("\r\n");
}

} // namespace

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
    // Flash only while an alarm in a flashing status is shown.
    const bool flashing =
        std::ranges::any_of(m_engine.pending(),
                            [this](const PendingAlarm& alarm)
                            {
                                const AlarmClass* c = m_engine.alarmClass(alarm.alarmClass);
                                return !alarm.shelved && c && c->colorsFor(alarm.status).flashing;
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
        const int count = int(rows(it->list).size());
        it->selected = std::clamp(it->selected, 0, std::max(0, count - 1));
        it->firstRow = std::clamp(it->firstRow, 0, std::max(0, count - 1));
    }
    Q_EMIT changed();
}

std::vector<AlarmRow> RuntimeAlarms::rows(AlarmList list) const
{
    std::vector<AlarmRow> result;
    const QLocale locale;
    const auto colorsOf = [this](const QString& name, AlarmStatus status)
    {
        const AlarmClass* c = m_engine.alarmClass(name);
        return c ? c->colorsFor(status) : AlarmColors{};
    };
    const auto priorityOf = [this](const QString& name)
    {
        const AlarmClass* c = m_engine.alarmClass(name);
        return c ? c->priority : 0;
    };
    const auto date = [&locale](const QDateTime& t)
    {
        return t.isValid() ? locale.toString(t.date(), QLocale::ShortFormat) : QString();
    };
    const auto time = [](const QDateTime& t)
    {
        return t.isValid() ? t.time().toString(QStringLiteral("HH:mm:ss")) : QString();
    };

    switch (list)
    {
    case AlarmList::Logged:
        for (const LoggedAlarm& entry : m_engine.log())
        {
            // A log line in the colours of its status; normal ones plain.
            AlarmRow row{entry.id,
                         date(entry.time),
                         time(entry.time),
                         entry.alarmClass,
                         entry.text,
                         m_engine.statusText(entry.status),
                         {},
                         {},
                         priorityOf(entry.alarmClass),
                         {}};
            if (entry.status != AlarmStatus::Normal)
            {
                const AlarmColors colors = colorsOf(entry.alarmClass, entry.status);
                row.colors = {colors.text, colors.background, false};
            }
            result.push_back(std::move(row));
        }
        break;
    case AlarmList::Statistics:
        for (const AlarmStatistics& s : m_engine.statistics())
        {
            AlarmRow row;
            row.id = s.id;
            row.text = s.text;
            const auto secs = [&locale](double v)
            {
                return locale.toString(v, 'f', 1);
            };
            row.statistics = {QString::number(s.frequency),       secs(s.sumActiveActive),
                              secs(s.sumActiveInactive),          secs(s.sumActiveAcknowledged),
                              secs(s.averageActiveActive()),      secs(s.averageActiveInactive()),
                              secs(s.averageActiveAcknowledged())};
            result.push_back(std::move(row));
        }
        break;
    case AlarmList::Defined:
        for (const DefinedAlarm& alarm : m_engine.defined())
        {
            const PendingAlarm* pending = m_engine.find(alarm.id);
            QString status = m_engine.statusText(alarm.status);
            if (!alarm.enabled)
                status = tr("Disabled");
            else if (alarm.shelved)
                status = alarm.status == AlarmStatus::Normal
                             ? tr("Shelved")
                             : tr("Shelved · %1").arg(m_engine.statusText(alarm.status));
            AlarmRow row{alarm.id,
                         pending ? date(pending->raised) : QString(),
                         pending ? time(pending->raised) : QString(),
                         alarm.alarmClass,
                         alarm.text,
                         status,
                         alarm.infoText,
                         alarm.origin,
                         alarm.priority,
                         pending && alarm.enabled && !alarm.shelved
                             ? colorsOf(alarm.alarmClass, alarm.status)
                             : AlarmColors{}};
            row.enabled = alarm.enabled;
            row.shelved = alarm.shelved;
            result.push_back(std::move(row));
        }
        break;
    case AlarmList::Active:
        for (const PendingAlarm& alarm : m_engine.pending())
        {
            if (alarm.shelved)
                continue;
            result.push_back({alarm.id, date(alarm.raised), time(alarm.raised), alarm.alarmClass,
                              alarm.text, m_engine.statusText(alarm.status), alarm.infoText,
                              alarm.origin, alarm.priority,
                              colorsOf(alarm.alarmClass, alarm.status)});
        }
        break;
    }
    return result;
}

int RuntimeAlarms::pendingCount() const
{
    return int(std::ranges::count_if(m_engine.pending(),
                                     [](const PendingAlarm& alarm) { return !alarm.shelved; }));
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

bool RuntimeAlarms::disable(int id)
{
    return m_engine.disable(id);
}

bool RuntimeAlarms::enable(int id)
{
    return m_engine.enable(id);
}

bool RuntimeAlarms::shelve(int id)
{
    return m_engine.shelve(id);
}

bool RuntimeAlarms::unshelve(int id)
{
    return m_engine.unshelve(id);
}

QString RuntimeAlarms::csvOf(AlarmList list) const
{
    QString csv;
    const std::vector<AlarmRow> lines = rows(list);
    if (list == AlarmList::Statistics)
    {
        csv += csvLine({tr("Alarm number"), tr("Alarm text"), tr("Frequency"),
                        tr("Sum active-active (s)"), tr("Sum active-inactive (s)"),
                        tr("Sum active-acknowledged (s)"), tr("Average active-active (s)"),
                        tr("Average active-inactive (s)"), tr("Average active-acknowledged (s)")});
        for (const AlarmRow& row : lines)
        {
            QStringList fields{QString::number(row.id), row.text};
            for (const QString& value : row.statistics)
                fields << value;
            csv += csvLine(fields);
        }
        return csv;
    }
    csv += csvLine({tr("Alarm number"), tr("Date"), tr("Time"), tr("Alarm class"), tr("Alarm text"),
                    tr("Status"), tr("Info text"), tr("Origin"), tr("Priority")});
    for (const AlarmRow& row : lines)
        csv += csvLine({QString::number(row.id), row.date, row.time, row.alarmClass, row.text,
                        row.status, row.infoText, row.origin, QString::number(row.priority)});
    return csv;
}

QString RuntimeAlarms::exportList(AlarmList list)
{
    static constexpr const char* kNames[] = {"ActiveAlarms", "LoggedAlarms", "AlarmStatistics",
                                             "DefinedAlarms"};
    const QString folder =
        m_exportFolder.isEmpty() ? QDir::current().absolutePath() : m_exportFolder;
    QDir().mkpath(folder);
    const QString path = QDir(folder).filePath(
        QStringLiteral("%1_%2.csv")
            .arg(QLatin1StringView(kNames[int(list)]),
                 QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
    QSaveFile file(path);
    // UTF-8 with a byte order mark, so spreadsheets read it as such.
    const bool ok = file.open(QIODevice::WriteOnly) && file.write("\xEF\xBB\xBF") == 3 &&
                    file.write(csvOf(list).toUtf8()) >= 0 && file.commit();
    Q_EMIT exported(ok ? path : QString());
    return ok ? path : QString();
}

void RuntimeAlarms::reset()
{
    m_views.clear();
    Q_EMIT changed();
}

} // namespace hmi
