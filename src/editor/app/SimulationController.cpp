#include "editor/app/SimulationController.h"

#include "comm/BindingEvaluator.h"
#include "comm/TagEngine.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "editor/tools/ToolContext.h"
#include "quick/RuntimeView.h"

namespace hmi
{

SimulationController::SimulationController(ToolContext& context, TagEngine& engine, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_engine(engine)
    , m_evaluator(std::make_unique<BindingEvaluator>(engine))
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
    m_engine.start(m_context.project().tags().tags());
    m_evaluator->show(*screen);
    showOnView();
    return true;
}

void SimulationController::end()
{
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
    if (m_running || !begin())
        return;
    m_running = true;
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
    m_view = view;
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
