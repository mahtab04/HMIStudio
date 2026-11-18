#pragma once

#include "quick/RuntimeInputHandler.h"
#include "quick/RuntimeView.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <QStringList>
#include <qqmlintegration.h>

#include <functional>
#include <memory>

namespace hmi
{

class AlarmEngine;
class BindingEvaluator;
class RuntimeAlarms;
class TagEngine;
class ToolContext;

// Runtime simulation, as in TIA Portal's Runtime Simulator: starting it
// simulates the project's tags and opens the current screen in a window of
// its own, at the screen's size, driven by its bindings. The editor stays
// editable; the simulation runs on a copy taken at start, so edits show
// after a restart and simulated values never reach the document.
class SimulationController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SimulationController is provided by the application")
    Q_PROPERTY(hmi::RuntimeInputHandler* input READ input CONSTANT)
    // Whether this run reads and writes PLC tags on their PLCs (the project
    // has PLC tags and they are not simulated).
    Q_PROPERTY(bool connectedToPlcs READ isConnectedToPlcs NOTIFY runningChanged)
    // Simulate PLC tags too, as internal tags, when no PLC is at hand; for
    // the next start.
    Q_PROPERTY(bool simulatePlcTags READ simulatePlcTags WRITE setSimulatePlcTags NOTIFY
                   simulatePlcTagsChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString screenTitle READ screenTitle NOTIFY screenChanged)
    Q_PROPERTY(QSizeF screenSize READ screenSize NOTIFY screenChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    // A popup screen shown over the current one (Show popup action).
    Q_PROPERTY(hmi::RuntimeInputHandler* popupInput READ popupInput CONSTANT)
    Q_PROPERTY(bool popupOpen READ popupOpen NOTIFY popupChanged)
    Q_PROPERTY(QString popupTitle READ popupTitle NOTIFY popupChanged)
    Q_PROPERTY(QSizeF popupSize READ popupSize NOTIFY popupChanged)
    // The alarm line of the simulator: the newest alarm still to
    // acknowledge, in its class colours, and how many there are.
    Q_PROPERTY(int unacknowledgedAlarms READ unacknowledgedAlarms NOTIFY alarmsChanged)
    Q_PROPERTY(QString newestAlarmText READ newestAlarmText NOTIFY alarmsChanged)
    Q_PROPERTY(QColor newestAlarmColor READ newestAlarmColor NOTIFY alarmsChanged)
    Q_PROPERTY(QColor newestAlarmTextColor READ newestAlarmTextColor NOTIFY alarmsChanged)
    // Where an alarm control's Export wrote its file, or why it could not.
    Q_PROPERTY(QString alarmExportMessage READ alarmExportMessage NOTIFY alarmExportChanged)

public:
    SimulationController(ToolContext& context, TagEngine& engine, QObject* parent = nullptr);
    ~SimulationController() override;
    SimulationController(const SimulationController&) = delete;
    SimulationController& operator=(const SimulationController&) = delete;
    SimulationController(SimulationController&&) = delete;
    SimulationController& operator=(SimulationController&&) = delete;

    // Starts with the screen the editor shows.
    Q_INVOKABLE void start();
    [[nodiscard]] bool isConnectedToPlcs() const noexcept { return m_connectedToPlcs; }
    [[nodiscard]] bool simulatePlcTags() const noexcept { return m_simulatePlcTags; }
    void setSimulatePlcTags(bool simulate);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    // Takes a new copy of the screen and the tags, keeping the window.
    Q_INVOKABLE void restart();
    // The simulator window's view; it shows the play copy until stop().
    Q_INVOKABLE void attachView(hmi::RuntimeView* view);
    // Shows another screen of the project (an Open screen action); tags keep
    // their values. Returns false, with a problem reported, for a screen the
    // project does not have.
    bool openScreen(const QString& name);
    // Shows a screen of the project as a popup over the current one,
    // replacing a popup that is open. Its objects work like the screen's.
    Q_INVOKABLE bool openPopup(const QString& name);
    Q_INVOKABLE void closePopup();
    // The popup's view, while the popup is open.
    Q_INVOKABLE void attachPopupView(hmi::RuntimeView* view);
    [[nodiscard]] RuntimeInputHandler* popupInput() const noexcept { return m_popupInput.get(); }
    [[nodiscard]] bool popupOpen() const;
    [[nodiscard]] QString popupTitle() const;
    [[nodiscard]] QSizeF popupSize() const;
    [[nodiscard]] const BindingEvaluator& popupEvaluator() const noexcept
    {
        return *m_popupEvaluator;
    }

    [[nodiscard]] RuntimeInputHandler* input() const noexcept { return m_input.get(); }
    [[nodiscard]] AlarmEngine& alarmEngine() const noexcept { return *m_alarmEngine; }
    [[nodiscard]] RuntimeAlarms& runtimeAlarms() const noexcept { return *m_alarms; }
    [[nodiscard]] int unacknowledgedAlarms() const;
    [[nodiscard]] QString newestAlarmText() const;
    [[nodiscard]] QColor newestAlarmColor() const;
    [[nodiscard]] QColor newestAlarmTextColor() const;
    // Acknowledges the alarm the alarm line shows.
    Q_INVOKABLE void acknowledgeNewestAlarm();
    [[nodiscard]] QString alarmExportMessage() const { return m_alarmExportMessage; }
    // Where alarm logs (kept across runs, per project) and alarm exports go;
    // empty keeps logs for one run and exports to the working folder.
    void setRuntimeFolder(const QString& folder);
    [[nodiscard]] const QString& runtimeFolder() const noexcept { return m_runtimeFolder; }
    // The saved project's file, which names its alarm log.
    void setDocumentPath(std::function<QString()> path) { m_documentPath = std::move(path); }
    // The alarm log file of the project, or empty.
    [[nodiscard]] QString alarmLogFile() const;
    // Where the runtime reads the native PLC libraries the user trusts.
    void setTrustedLibraries(std::function<QStringList()> trusted)
    {
        m_trustedLibraries = std::move(trusted);
    }
    [[nodiscard]] bool isRunning() const noexcept { return m_running; }
    [[nodiscard]] QString screenTitle() const;
    [[nodiscard]] QSizeF screenSize() const;
    [[nodiscard]] QStringList problems() const;
    [[nodiscard]] const BindingEvaluator& evaluator() const noexcept { return *m_evaluator; }

Q_SIGNALS:
    void runningChanged();
    // Another screen is shown (start, restart, Open screen).
    void screenChanged();
    // A popup opened, closed or was replaced.
    void popupChanged();
    // Pending alarms changed (or flash).
    void alarmsChanged();
    void alarmExportChanged();
    void simulatePlcTagsChanged();
    void problemsChanged();

private:
    bool begin();
    // Shows `screen` in play: the evaluator's copy, the input and the view.
    void show(const Screen& screen);
    void end();
    void showOnView();
    void showOnPopupView();
    // Redraws the alarm controls of the screen and the popup.
    void refreshAlarmControls();
    [[nodiscard]] const struct PendingAlarm* newestUnacknowledged() const;

    ToolContext& m_context;
    TagEngine& m_engine;
    std::unique_ptr<AlarmEngine> m_alarmEngine;
    std::unique_ptr<RuntimeAlarms> m_alarms;
    std::unique_ptr<BindingEvaluator> m_evaluator;
    std::unique_ptr<RuntimeInputHandler> m_input;
    std::unique_ptr<BindingEvaluator> m_popupEvaluator;
    std::unique_ptr<RuntimeInputHandler> m_popupInput;
    QPointer<RuntimeView> m_popupView;
    QPointer<RuntimeView> m_view;
    QString m_runtimeFolder;
    std::function<QString()> m_documentPath;
    QString m_alarmExportMessage;
    std::function<QStringList()> m_trustedLibraries;
    bool m_connectedToPlcs = false;
    bool m_simulatePlcTags = false;
    bool m_running = false;
};

} // namespace hmi
