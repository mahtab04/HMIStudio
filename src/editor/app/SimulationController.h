#pragma once

#include "quick/RuntimeInputHandler.h"
#include "quick/RuntimeView.h"

#include <QObject>
#include <QPointer>
#include <QSizeF>
#include <QStringList>
#include <qqmlintegration.h>

#include <functional>
#include <memory>

namespace hmi
{

class BindingEvaluator;
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
    Q_PROPERTY(bool monitoring READ isMonitoring NOTIFY runningChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString screenTitle READ screenTitle NOTIFY screenChanged)
    Q_PROPERTY(QSizeF screenSize READ screenSize NOTIFY screenChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    // A popup screen shown over the current one (Show popup action).
    Q_PROPERTY(hmi::RuntimeInputHandler* popupInput READ popupInput CONSTANT)
    Q_PROPERTY(bool popupOpen READ popupOpen NOTIFY popupChanged)
    Q_PROPERTY(QString popupTitle READ popupTitle NOTIFY popupChanged)
    Q_PROPERTY(QSizeF popupSize READ popupSize NOTIFY popupChanged)

public:
    SimulationController(ToolContext& context, TagEngine& engine, QObject* parent = nullptr);
    ~SimulationController() override;
    SimulationController(const SimulationController&) = delete;
    SimulationController& operator=(const SimulationController&) = delete;
    SimulationController(SimulationController&&) = delete;
    SimulationController& operator=(SimulationController&&) = delete;

    // Starts with the screen the editor shows.
    Q_INVOKABLE void start();
    Q_INVOKABLE void startMonitoring();
    [[nodiscard]] bool isMonitoring() const noexcept { return m_monitoring; }
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
    // Where PLC monitoring reads the native libraries the user trusts.
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
    void problemsChanged();

private:
    bool begin();
    // Shows `screen` in play: the evaluator's copy, the input and the view.
    void show(const Screen& screen);
    void end();
    void showOnView();
    void showOnPopupView();

    ToolContext& m_context;
    TagEngine& m_engine;
    std::unique_ptr<BindingEvaluator> m_evaluator;
    std::unique_ptr<RuntimeInputHandler> m_input;
    std::unique_ptr<BindingEvaluator> m_popupEvaluator;
    std::unique_ptr<RuntimeInputHandler> m_popupInput;
    QPointer<RuntimeView> m_popupView;
    QPointer<RuntimeView> m_view;
    std::function<QStringList()> m_trustedLibraries;
    bool m_monitoring = false;
    bool m_running = false;
};

} // namespace hmi
