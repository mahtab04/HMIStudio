#pragma once

#include "core/tags/Tag.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include <memory>
#include <span>
#include <vector>

namespace hmi
{

class IDriver;
class DriverHost;

// Runs the tag sources and holds the current value of every tag (Facade
// over drivers and threading, ARCHITECTURE.md section 8). Drivers run on a
// worker thread and are polled every kTickMs; their values reach the GUI
// thread queued and are announced at most every kPublishMs (about 15 Hz)
// as one valuesChanged with the names that changed, so a fast source
// cannot flood repainting.
class TagEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int kTickMs = 50;
    static constexpr int kPublishMs = 66;

    explicit TagEngine(QObject* parent = nullptr);
    ~TagEngine() override;
    TagEngine(const TagEngine&) = delete;
    TagEngine& operator=(const TagEngine&) = delete;
    TagEngine(TagEngine&&) = delete;
    TagEngine& operator=(TagEngine&&) = delete;

    // Starts serving `tags` from `driver` (the simulator when null). Every
    // tag starts at its start value, announced at once.
    void start(std::span<const Tag> tags, std::unique_ptr<IDriver> driver = {});
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_running; }

    // The current value; empty for unknown tags or while stopped.
    [[nodiscard]] TagValue value(const QString& tag) const;
    // Sets a tag through its driver; the new value is shown at once.
    void write(const QString& tag, const TagValue& value);

Q_SIGNALS:
    // Names of the tags whose value changed since the last announcement.
    void valuesChanged(const QStringList& tags);
    void runningChanged();

private:
    void receive(const QHash<QString, TagValue>& values);
    void publish();

    QThread m_thread;
    DriverHost* m_host = nullptr;      // lives in m_thread
    QHash<QString, TagValue> m_values; // by lower-case name
    QHash<QString, QString> m_names;   // lower-case name to the tag's own spelling
    QHash<QString, Tag> m_tags;        // by lower-case name, for conversions
    QSet<QString> m_dirty;
    QTimer m_publishTimer;
    bool m_running = false;
};

} // namespace hmi
