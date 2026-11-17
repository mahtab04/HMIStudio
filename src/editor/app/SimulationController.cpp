#include "editor/app/SimulationController.h"

#include "comm/BindingEvaluator.h"
#include "comm/PlcDriver.h"
#include "comm/TagEngine.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "editor/app/FillingStationDriver.h"
#include "editor/tools/ToolContext.h"
#include "quick/RuntimeInputHandler.h"
#include "quick/RuntimeView.h"

namespace hmi
{

SimulationController::SimulationController(ToolContext& context, TagEngine& engine, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_engine(engine)
    , m_evaluator(std::make_unique<BindingEvaluator>(engine))
    , m_input(std::make_unique<RuntimeInputHandler>(engine))
{
    connect(m_evaluator.get(), &BindingEvaluator::problemsChanged, this,
            &SimulationController::problemsChanged);
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
    std::unique_ptr<IDriver> driver;
    if (m_monitoring)
    {
        const auto connections = m_context.project().connections();
        driver = std::make_unique<PlcDriver>(
            std::vector<Connection>(connections.begin(), connections.end()),
            m_trustedLibraries ? m_trustedLibraries() : QStringList());
    }
    else if (m_context.project().simulationProfile() == u"filling-station")
        driver = std::make_unique<FillingStationDriver>();
    m_engine.start(m_context.project().tags().tags(), std::move(driver));
    m_evaluator->show(*screen);
    m_input->setScreen(m_evaluator->playScreen());
    showOnView();
    return true;
}

void SimulationController::end()
{
    m_input->setScreen(nullptr);
    // The view lets go of the play copy before the evaluator drops it: it
    // disconnects from that copy's notifier.
    if (m_view)
        m_view->setScreen(nullptr, nullptr);
    m_evaluator->clear();
    m_engine.stop();
}

void SimulationController::showOnView()
{
    if (!m_view)
        return;
    m_view->setGraphics(m_context.graphics());
    m_view->setScreen(m_evaluator->playScreen(),
                      m_evaluator->playScreen() ? &m_evaluator->playProject().notifier() : nullptr);
}

void SimulationController::start()
{
    if (m_running)
        return;
    m_monitoring = false;
    if (!begin())
        return;
    m_running = true;
    Q_EMIT runningChanged();
}

void SimulationController::startMonitoring()
{
    if (m_running)
        end();
    m_monitoring = true;
    m_running = begin();
    Q_EMIT runningChanged();
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
    return m_running ? m_evaluator->problems() : QStringList();
}

} // namespace hmi
