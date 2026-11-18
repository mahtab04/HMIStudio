#include "editor/app/SimulationController.h"

#include "comm/AlarmEngine.h"
#include "comm/BindingEvaluator.h"
#include "comm/PlcDriver.h"
#include "comm/RuntimeAlarms.h"
#include "comm/RuntimeDriver.h"
#include "comm/SimulatorDriver.h"
#include "comm/TagEngine.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/property/Prop.h"
#include "editor/app/FillingStationDriver.h"
#include "editor/tools/ToolContext.h"
#include "quick/RuntimeInputHandler.h"
#include "quick/RuntimeView.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace hmi
{

SimulationController::SimulationController(ToolContext& context, TagEngine& engine, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_engine(engine)
    , m_alarmEngine(std::make_unique<AlarmEngine>(engine))
    , m_alarms(std::make_unique<RuntimeAlarms>(*m_alarmEngine))
    , m_evaluator(std::make_unique<BindingEvaluator>(engine))
    , m_input(std::make_unique<RuntimeInputHandler>(engine))
    , m_popupEvaluator(std::make_unique<BindingEvaluator>(engine))
    , m_popupInput(std::make_unique<RuntimeInputHandler>(engine))
{
    connect(m_popupEvaluator.get(), &BindingEvaluator::problemsChanged, this,
            &SimulationController::problemsChanged);
    // Alarm controls on the screen and in the popup operate the alarms.
    m_input->setAlarms(m_alarms.get());
    m_popupInput->setAlarms(m_alarms.get());
    connect(m_alarms.get(), &RuntimeAlarms::changed, this,
            [this]
            {
                refreshAlarmControls();
                Q_EMIT alarmsChanged();
            });
    connect(m_alarms.get(), &RuntimeAlarms::exported, this,
            [this](const QString& path)
            {
                m_alarmExportMessage =
                    path.isEmpty()
                        ? tr("The alarms could not be exported.")
                        : tr("Alarms exported to %1").arg(QDir::toNativeSeparators(path));
                Q_EMIT alarmExportChanged();
            });
    // Screen and popup actions, from the screen and from the popup, are
    // queued: the action list that asked runs to its end first.
    for (RuntimeInputHandler* input : {m_input.get(), m_popupInput.get()})
    {
        connect(input, &RuntimeInputHandler::popupRequested, this, &SimulationController::openPopup,
                Qt::QueuedConnection);
        connect(input, &RuntimeInputHandler::popupCloseRequested, this,
                &SimulationController::closePopup, Qt::QueuedConnection);
    }
    connect(m_popupInput.get(), &RuntimeInputHandler::screenRequested, this,
            &SimulationController::openScreen, Qt::QueuedConnection);
    connect(m_evaluator.get(), &BindingEvaluator::problemsChanged, this,
            &SimulationController::problemsChanged);
    // Queued: the action list that asked runs to its end on the old screen.
    connect(m_input.get(), &RuntimeInputHandler::screenRequested, this,
            &SimulationController::openScreen, Qt::QueuedConnection);
    // A new or opened document ends the simulation.
    connect(&m_context.project().notifier(), &DocumentNotifier::projectReset, this,
            &SimulationController::stop);
}

SimulationController::~SimulationController()
{
    end();
}

bool SimulationController::begin()
{
    const Screen* screen = m_context.screen();
    if (!screen)
        return false;
    // Internal tags: simulated (or the training profile). PLC tags: read
    // and written on their PLCs, unless they are simulated as well.
    std::unique_ptr<IDriver> driver;
    if (m_context.project().simulationProfile() == u"filling-station")
        driver = std::make_unique<FillingStationDriver>();
    else
        driver = std::make_unique<SimulatorDriver>();
    const auto tags = m_context.project().tags().tags();
    m_connectedToPlcs =
        !m_simulatePlcTags &&
        std::ranges::any_of(tags, [](const Tag& tag) { return !tag.connection.isEmpty(); });
    if (m_connectedToPlcs)
    {
        const auto connections = m_context.project().connections();
        driver = std::make_unique<RuntimeDriver>(
            std::move(driver), std::make_unique<PlcDriver>(
                                   std::vector<Connection>(connections.begin(), connections.end()),
                                   m_trustedLibraries ? m_trustedLibraries() : QStringList()));
    }
    m_engine.start(tags, std::move(driver));
    m_alarms->reset();
    m_alarmEngine->setLogFile(alarmLogFile());
    m_alarmEngine->start(m_context.project().alarms());
    if (!m_alarmExportMessage.isEmpty())
    {
        m_alarmExportMessage.clear();
        Q_EMIT alarmExportChanged();
    }
    show(*screen);
    return true;
}

void SimulationController::show(const Screen& screen)
{
    // Another screen closes the popup.
    closePopup();
    // Input and view let go of the old play copy before it is replaced.
    m_input->setScreen(nullptr);
    if (m_view)
        m_view->setScreen(nullptr, nullptr);
    m_evaluator->show(screen, &m_context.project());
    m_input->setScreen(m_evaluator->playScreen());
    showOnView();
    Q_EMIT screenChanged();
}

bool SimulationController::openScreen(const QString& name)
{
    if (!m_running)
        return false;
    for (const auto& screen : m_context.project().screens())
    {
        if (screen->name().compare(name, Qt::CaseInsensitive) == 0)
        {
            show(*screen);
            return true;
        }
    }
    m_evaluator->report(tr("Open screen: the project has no screen called %1.").arg(name));
    return false;
}

bool SimulationController::openPopup(const QString& name)
{
    if (!m_running)
        return false;
    for (const auto& screen : m_context.project().screens())
    {
        if (screen->name().compare(name, Qt::CaseInsensitive) != 0)
            continue;
        // Input and view let go of a popup being replaced first.
        m_popupInput->setScreen(nullptr);
        if (m_popupView)
            m_popupView->setScreen(nullptr, nullptr);
        m_popupEvaluator->show(*screen, &m_context.project());
        m_popupInput->setScreen(m_popupEvaluator->playScreen());
        // While the popup is open the screen behind does not take input.
        m_input->cancel();
        showOnPopupView();
        Q_EMIT popupChanged();
        return true;
    }
    m_evaluator->report(tr("Show popup: the project has no screen called %1.").arg(name));
    return false;
}

void SimulationController::closePopup()
{
    if (!m_popupEvaluator->playScreen())
        return;
    m_popupInput->setScreen(nullptr);
    if (m_popupView)
        m_popupView->setScreen(nullptr, nullptr);
    m_popupEvaluator->clear();
    Q_EMIT popupChanged();
}

bool SimulationController::popupOpen() const
{
    return m_popupEvaluator->playScreen() != nullptr;
}

QString SimulationController::popupTitle() const
{
    const Screen* screen = m_popupEvaluator->playScreen();
    return screen ? screen->name() : QString();
}

QSizeF SimulationController::popupSize() const
{
    const Screen* screen = m_popupEvaluator->playScreen();
    return screen ? screen->size() : QSizeF();
}

void SimulationController::attachPopupView(RuntimeView* view)
{
    if (m_popupView && m_popupView != view)
        m_popupView->setScreen(nullptr, nullptr);
    if (m_popupView)
        disconnect(m_popupView, nullptr, m_popupInput.get(), nullptr);
    m_popupView = view;
    if (m_popupView)
    {
        connect(m_popupView, &RuntimeView::gesture, m_popupInput.get(),
                &RuntimeInputHandler::handleGesture);
        connect(m_popupView, &RuntimeView::keyInput, m_popupInput.get(),
                &RuntimeInputHandler::keyInput);
        connect(m_popupView, &RuntimeView::inputCancelled, m_popupInput.get(),
                &RuntimeInputHandler::cancel);
    }
    showOnPopupView();
}

void SimulationController::showOnPopupView()
{
    if (!m_popupView)
        return;
    m_popupView->setGraphics(m_context.graphics());
    m_popupView->setAlarms(m_alarms.get());
    const Screen* screen = m_popupEvaluator->playScreen();
    m_popupView->setScreen(screen, screen ? &m_popupEvaluator->playProject().notifier() : nullptr);
}

void SimulationController::end()
{
    closePopup();
    m_input->setScreen(nullptr);
    // The view lets go of the play copy before the evaluator drops it: it
    // disconnects from that copy's notifier.
    if (m_view)
        m_view->setScreen(nullptr, nullptr);
    m_evaluator->clear();
    m_engine.stop();
}

void SimulationController::refreshAlarmControls()
{
    // An alarm control is redrawn through its play copy's notifier, like a
    // driven property; inside a group or window its top-level object is.
    const auto refresh = [](const BindingEvaluator& evaluator)
    {
        const Screen* screen = evaluator.playScreen();
        if (!screen)
            return;
        const std::function<bool(const HmiObject&)> drawsAlarms = [&](const HmiObject& object)
        {
            return object.usesAlarms() || std::ranges::any_of(object.children(), [&](const auto& c)
                                                              { return drawsAlarms(*c); });
        };
        for (const auto& object : screen->objects())
        {
            if (drawsAlarms(*object))
                Q_EMIT evaluator.playProject().notifier().propertyChanged(object->id(),
                                                                          Prop::Columns);
        }
    };
    refresh(*m_evaluator);
    refresh(*m_popupEvaluator);
}

const PendingAlarm* SimulationController::newestUnacknowledged() const
{
    if (!m_running)
        return nullptr;
    for (const PendingAlarm& alarm : m_alarmEngine->pending())
    {
        // Shelved alarms stay off the alarm line.
        if (m_alarmEngine->needsAcknowledgment(alarm))
            return &alarm;
    }
    return nullptr;
}

void SimulationController::setRuntimeFolder(const QString& folder)
{
    m_runtimeFolder = folder;
    m_alarms->setExportFolder(folder.isEmpty() ? QString()
                                               : QDir(folder).filePath(QStringLiteral("exports")));
}

QString SimulationController::alarmLogFile() const
{
    if (m_runtimeFolder.isEmpty())
        return {};
    // One log per project: its name, and its file so that two projects of
    // the same name keep apart.
    QString name = m_context.project().name();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    if (name.isEmpty())
        name = QStringLiteral("project");
    const QString path = m_documentPath ? m_documentPath() : QString();
    const QString key =
        path.isEmpty() ? QStringLiteral("unsaved")
                       : QString::fromLatin1(
                             QCryptographicHash::hash(QFileInfo(path).absoluteFilePath().toUtf8(),
                                                      QCryptographicHash::Sha1)
                                 .toHex()
                                 .left(10));
    return QDir(m_runtimeFolder).filePath(QStringLiteral("alarm-logs/%1_%2.json").arg(name, key));
}

int SimulationController::unacknowledgedAlarms() const
{
    return m_running ? m_alarmEngine->unacknowledgedCount() : 0;
}

QString SimulationController::newestAlarmText() const
{
    const PendingAlarm* alarm = newestUnacknowledged();
    return alarm ? QStringLiteral("%1  %2  %3")
                       .arg(alarm->raised.time().toString(QStringLiteral("HH:mm:ss")),
                            alarm->alarmClass, alarm->text)
                 : QString();
}

QColor SimulationController::newestAlarmColor() const
{
    const PendingAlarm* alarm = newestUnacknowledged();
    const AlarmClass* c = alarm ? m_alarmEngine->alarmClass(alarm->alarmClass) : nullptr;
    if (!c)
        return {};
    const AlarmColors& colors = c->colorsFor(alarm->status);
    return colors.flashing && !m_alarms->flashOn() ? QColor(Qt::white) : colors.background;
}

QColor SimulationController::newestAlarmTextColor() const
{
    const PendingAlarm* alarm = newestUnacknowledged();
    const AlarmClass* c = alarm ? m_alarmEngine->alarmClass(alarm->alarmClass) : nullptr;
    if (!c)
        return {};
    const AlarmColors& colors = c->colorsFor(alarm->status);
    return colors.flashing && !m_alarms->flashOn() ? QColor(0x1F, 0x23, 0x28) : colors.text;
}

void SimulationController::acknowledgeNewestAlarm()
{
    if (const PendingAlarm* alarm = newestUnacknowledged())
        m_alarmEngine->acknowledge(alarm->id);
}

void SimulationController::showOnView()
{
    if (!m_view)
        return;
    m_view->setGraphics(m_context.graphics());
    m_view->setAlarms(m_alarms.get());
    m_view->setScreen(m_evaluator->playScreen(),
                      m_evaluator->playScreen() ? &m_evaluator->playProject().notifier() : nullptr);
}

void SimulationController::start()
{
    if (m_running)
        return;
    if (!begin())
        return;
    m_running = true;
    Q_EMIT runningChanged();
}

void SimulationController::setSimulatePlcTags(bool simulate)
{
    if (simulate == m_simulatePlcTags)
        return;
    m_simulatePlcTags = simulate;
    Q_EMIT simulatePlcTagsChanged();
}

void SimulationController::stop()
{
    if (!m_running)
        return;
    end();
    m_running = false;
    Q_EMIT runningChanged();
    Q_EMIT problemsChanged();
}

void SimulationController::toggle()
{
    if (m_running)
        stop();
    else
        start();
}

void SimulationController::restart()
{
    if (!m_running)
    {
        start();
        return;
    }
    end();
    if (!begin())
    {
        m_running = false;
        Q_EMIT problemsChanged();
    }
    Q_EMIT runningChanged();
}

void SimulationController::attachView(RuntimeView* view)
{
    if (m_view && m_view != view)
        m_view->setScreen(nullptr, nullptr);
    if (m_view)
        disconnect(m_view, nullptr, m_input.get(), nullptr);
    m_view = view;
    if (m_view)
    {
        connect(m_view, &RuntimeView::gesture, m_input.get(), &RuntimeInputHandler::handleGesture);
        connect(m_view, &RuntimeView::keyInput, m_input.get(), &RuntimeInputHandler::keyInput);
        connect(m_view, &RuntimeView::inputCancelled, m_input.get(), &RuntimeInputHandler::cancel);
    }
    showOnView();
}

QString SimulationController::screenTitle() const
{
    const Screen* screen = m_evaluator->playScreen();
    return screen ? screen->name() : QString();
}

QSizeF SimulationController::screenSize() const
{
    const Screen* screen = m_evaluator->playScreen();
    return screen ? screen->size() : QSizeF();
}

QStringList SimulationController::problems() const
{
    return m_running ? m_evaluator->problems() + m_popupEvaluator->problems() : QStringList();
}

} // namespace hmi
