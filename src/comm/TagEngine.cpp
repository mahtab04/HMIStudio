#include "comm/TagEngine.h"

#include "comm/SimulatorDriver.h"

#include <cmath>

namespace hmi
{

// Owns the driver on the worker thread and polls it. Created and deleted
// from the GUI thread, used only on the worker thread in between.
class DriverHost : public QObject
{
public:
    DriverHost(std::unique_ptr<IDriver> driver, std::vector<Tag> tags)
        : m_driver(std::move(driver))
        , m_tags(std::move(tags))
    {
    }

    void begin()
    {
        if (auto started = m_driver->start(m_tags); !started)
            qWarning("TagEngine: %s", qPrintable(started.error().message));
        m_clock.start();
        m_tick = new QTimer(this);
        m_tick->setInterval(TagEngine::kTickMs);
        connect(m_tick, &QTimer::timeout, this, &DriverHost::tick);
        m_tick->start();
    }

    void end()
    {
        if (m_tick)
            m_tick->stop();
        m_driver->stop();
    }

    void write(const QString& tag, const TagValue& value)
    {
        if (auto written = m_driver->write(tag, value); !written)
            qWarning("TagEngine: %s", qPrintable(written.error().message));
    }

private:
    void tick()
    {
        QHash<QString, TagValue> batch;
        m_driver->poll(double(m_clock.elapsed()) / 1000.0,
                       [&batch](const QString& tag, const TagValue& value)
                       { batch.insert(tag, value); });
        // TagEngine queues the batch into the GUI thread.
        if (ready)
        {
            QHash<QString, QString> errors;
            for (const Tag& tag : m_tags)
                errors.insert(tag.name.toLower(), m_driver->problem(tag.name));
            ready(batch, errors);
        }
    }

public:
    // Called with every batch (queued into the GUI thread by TagEngine).
    std::function<void(const QHash<QString, TagValue>&, const QHash<QString, QString>&)> ready;

private:
    std::unique_ptr<IDriver> m_driver;
    std::vector<Tag> m_tags;
    QElapsedTimer m_clock;
    QTimer* m_tick = nullptr;
};

TagEngine::TagEngine(QObject* parent)
    : QObject(parent)
{
    m_publishTimer.setInterval(kPublishMs);
    connect(&m_publishTimer, &QTimer::timeout, this, &TagEngine::publish);
}

TagEngine::~TagEngine()
{
    stop();
}

void TagEngine::start(std::span<const Tag> tags, std::unique_ptr<IDriver> driver)
{
    stop();
    const quint64 generation = ++m_generation;
    if (!driver)
        driver = std::make_unique<SimulatorDriver>();
    m_simulating = driver->isSimulation();
    m_readOnly = driver->isReadOnly();
    std::vector<Tag> copy(tags.begin(), tags.end());
    for (const Tag& tag : copy)
    {
        const QString key = tag.name.toLower();
        m_tags.insert(key, tag);
        m_names.insert(key, tag.name);
        m_values.insert(key,
                        !m_readOnly || tag.connection.isEmpty() ? tag.startValue() : TagValue());
        m_dirty.insert(key);
    }

    m_host = new DriverHost(std::move(driver), std::move(copy));
    m_host->ready = [this, generation](const QHash<QString, TagValue>& batch,
                                       const QHash<QString, QString>& errors)
    {
        QMetaObject::invokeMethod(
            this, [this, generation, batch, errors] { receive(generation, batch, errors); },
            Qt::QueuedConnection);
    };
    m_host->moveToThread(&m_thread);
    m_thread.start();
    QMetaObject::invokeMethod(m_host, [host = m_host] { host->begin(); }, Qt::QueuedConnection);

    m_running = true;
    m_publishTimer.start();
    publish(); // start values at once
    Q_EMIT runningChanged();
}

void TagEngine::stop()
{
    if (!m_running)
        return;
    m_publishTimer.stop();
    QMetaObject::invokeMethod(
        m_host, [host = m_host] { host->end(); }, Qt::BlockingQueuedConnection);
    m_host->ready = {};
    QMetaObject::invokeMethod(
        m_host, [host = m_host] { host->deleteLater(); }, Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    m_host = nullptr;
    ++m_generation;
    m_errors.clear();
    m_received.clear();
    m_values.clear();
    m_names.clear();
    m_tags.clear();
    m_dirty.clear();
    m_running = false;
    Q_EMIT runningChanged();
}

TagValue TagEngine::value(const QString& tag) const
{
    return m_values.value(tag.toLower());
}

void TagEngine::write(const QString& tag, const TagValue& value)
{
    const QString key = tag.toLower();
    if (!m_running || !m_tags.contains(key))
        return;
    if (m_readOnly)
        return;
    const TagValue converted = m_tags.value(key).converted(value);
    if (m_values.value(key) != converted)
    {
        m_values.insert(key, converted);
        m_dirty.insert(key);
    }
    QMetaObject::invokeMethod(
        m_host, [host = m_host, name = m_names.value(key), converted]
        { host->write(name, converted); }, Qt::QueuedConnection);
}

const Tag* TagEngine::definition(const QString& tag) const
{
    const auto it = m_tags.constFind(tag.toLower());
    return it == m_tags.cend() ? nullptr : &*it;
}

Result<void> TagEngine::writeChecked(const QString& name, const TagValue& value)
{
    if (!m_running)
        return makeError(ErrorCode::InvalidArgument, tr("The runtime is stopped."));
    if (m_readOnly)
        return makeError(ErrorCode::InvalidArgument,
                         tr("PLC monitoring is read only. Use Simulation to operate controls."));
    const Tag* tag = definition(name);
    if (!tag)
        return makeError(ErrorCode::NotFound, tr("Tag '%1' is missing.").arg(name));
    TagValue converted;
    if (tag->type == Tag::Type::String)
        converted = TagValues::toText(value);
    else if (tag->type == Tag::Type::Bool)
    {
        const QString text = TagValues::toText(value).trimmed().toLower();
        if (text != u"true" && text != u"false" && text != u"1" && text != u"0")
            return makeError(ErrorCode::TypeMismatch,
                             tr("Enter true, false, 1 or 0 for '%1'.").arg(name));
        converted = text == u"true" || text == u"1";
    }
    else
    {
        const auto number = TagValues::toNumber(value);
        if (!number || !std::isfinite(*number))
            return makeError(ErrorCode::TypeMismatch,
                             tr("Enter a finite number for '%1'.").arg(name));
        if (*number < tag->minimum || *number > tag->maximum)
            return makeError(
                ErrorCode::OutOfRange,
                tr("%1 must be between %2 and %3.").arg(name).arg(tag->minimum).arg(tag->maximum));
        if (tag->type == Tag::Type::Int)
        {
            // Preserve exact integer text, including values beyond 2^53.
            if (const auto* integer = std::get_if<qint64>(&value))
                converted = *integer;
            else if (const auto* text = std::get_if<QString>(&value))
            {
                bool ok = false;
                const qint64 parsedInteger = text->trimmed().toLongLong(&ok);
                if (!ok)
                    return makeError(ErrorCode::TypeMismatch,
                                     tr("Enter a whole number for '%1'.").arg(name));
                converted = parsedInteger;
            }
            else
            {
                if (*number >= 9223372036854775808.0 || *number < -9223372036854775808.0)
                    return makeError(ErrorCode::OutOfRange,
                                     tr("Integer is outside the supported range."));
                converted = qint64(std::llround(*number));
            }
        }
        else
            converted = *number;
        const double stored = TagValues::toNumber(converted).value_or(*number);
        if (stored < tag->minimum || stored > tag->maximum)
            return makeError(ErrorCode::OutOfRange,
                             tr("The rounded integer is outside the tag's limits."));
    }
    write(name, converted);
    return {};
}

QString TagEngine::quality(const QString& tag) const
{
    const QString key = tag.toLower();
    if (!m_running)
        return tr("Offline");
    if (m_simulating)
        return tr("Simulated");
    if (!m_errors.value(key).isEmpty())
        return tr("Bad");
    return m_received.contains(key) ? tr("Good") : tr("Waiting");
}

QString TagEngine::problem(const QString& tag) const
{
    return m_errors.value(tag.toLower());
}

void TagEngine::receive(quint64 generation, const QHash<QString, TagValue>& values,
                        const QHash<QString, QString>& errors)
{
    if (!m_running || generation != m_generation)
        return;
    for (auto it = errors.cbegin(); it != errors.cend(); ++it)
    {
        if (m_tags.contains(it.key()) && m_errors.value(it.key()) != it.value())
        {
            m_errors.insert(it.key(), it.value());
            m_dirty.insert(it.key());
        }
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        const QString key = it.key().toLower();
        if (!m_tags.contains(key))
            continue;
        if (!m_received.contains(key))
        {
            m_received.insert(key);
            m_dirty.insert(key);
        }
        if (m_values.value(key) != it.value())
        {
            m_values.insert(key, it.value());
            m_dirty.insert(key);
        }
    }
}

void TagEngine::publish()
{
    if (m_dirty.isEmpty())
        return;
    QStringList names;
    names.reserve(m_dirty.size());
    for (const QString& key : std::as_const(m_dirty))
        names << m_names.value(key, key);
    m_dirty.clear();
    Q_EMIT valuesChanged(names);
}

} // namespace hmi
