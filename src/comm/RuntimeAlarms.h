#pragma once

#include "comm/AlarmEngine.h"
#include "core/alarms/IAlarmSource.h"

#include <QHash>
#include <QObject>
#include <QTimer>

namespace hmi
{

// The alarm engine as alarm controls see it (IAlarmSource): lines with
// dates, times, status texts and class colours; per-control view state; a
// flash phase that alternates while a flashing alarm is pending; export of
// a list to CSV.
class RuntimeAlarms : public QObject, public IAlarmSource
{
    Q_OBJECT

public:
    static constexpr int kFlashMs = 500;

    explicit RuntimeAlarms(AlarmEngine& engine, QObject* parent = nullptr);

    [[nodiscard]] std::vector<AlarmRow> rows(AlarmList list) const override;
    [[nodiscard]] int pendingCount() const override;
    [[nodiscard]] int unacknowledgedCount() const override;
    [[nodiscard]] bool flashOn() const override { return m_flashOn; }
    [[nodiscard]] AlarmViewState viewState(ObjectId control) const override;
    void setViewState(ObjectId control, const AlarmViewState& state) override;
    bool acknowledge(int id) override;
    int acknowledge(const std::vector<int>& ids) override;
    bool confirm(int id) override;
    bool disable(int id) override;
    bool enable(int id) override;
    bool shelve(int id) override;
    bool unshelve(int id) override;
    QString exportList(AlarmList list) override;

    // Where exported files go.
    void setExportFolder(const QString& folder) { m_exportFolder = folder; }
    [[nodiscard]] const QString& exportFolder() const noexcept { return m_exportFolder; }
    // The CSV text of a list, as exported.
    [[nodiscard]] QString csvOf(AlarmList list) const;

    // Forgets the view states (a new screen or a new run).
    void reset();

Q_SIGNALS:
    // What alarm controls show changed: alarms, a view state or the flash.
    void changed();
    // A list was exported to this file, or failed to (empty path).
    void exported(const QString& path);

private:
    void onAlarmsChanged();

    AlarmEngine& m_engine;
    QHash<ObjectId, AlarmViewState> m_views;
    QTimer m_flash;
    bool m_flashOn = true;
    QString m_exportFolder;
};

} // namespace hmi
