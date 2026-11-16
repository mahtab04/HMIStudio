#include "comm/TagEngine.h"

#include "comm/SimulatorDriver.h"

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
        if (!batch.isEmpty() && ready)
            ready(batch);
    }

public:
    // Called with every batch (queued into the GUI thread by TagEngine).
    std::function<void(const QHash<QString, TagValue>&)> ready;

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
    if (!driver)
        driver = std::make_unique<SimulatorDriver>();
    std::vector<Tag> copy(tags.begin(), tags.end());
    for (const Tag& tag : copy)
    {
        const QString key = tag.name.toLower();
        m_tags.insert(key, tag);
        m_names.insert(key, tag.name);
        m_values.insert(key, tag.startValue());
        m_dirty.insert(key);
    }

    m_host = new DriverHost(std::move(driver), std::move(copy));
    m_host->ready = [this](const QHash<QString, TagValue>& batch)
    {
        QMetaObject::invokeMethod(this, [this, batch] { receive(batch); }, Qt::QueuedConnection);
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

void TagEngine::receive(const QHash<QString, TagValue>& values)
{
    if (!m_running)
        return;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        const QString key = it.key().toLower();
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
