#include "editor/app/EditorController.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/property/Prop.h"
#include "core/property/PropertyDescriptor.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/app/EditActions.h"
#include "editor/commands/ScreenCommands.h"
#include "editor/commands/SetPropertyCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/tools/ToolContext.h"
#include "editor/tools/ToolController.h"
#include "quick/CanvasItem.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QUndoStack>

#include <cmath>

namespace hmi
{

EditorController::EditorController(ToolContext& context, EditActions& actions,
                                   ToolboxDragController& toolboxDrag, QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_actions(actions)
    , m_toolboxDrag(toolboxDrag)
    , m_tools(new ToolController(context, this))
{
    connect(m_tools, &ToolController::toolChanged, this, &EditorController::toolChanged);
    connect(&m_context.selection(), &SelectionModel::changed, this,
            &EditorController::selectionChanged);
    connect(&m_context.selection(), &SelectionModel::multiSelectModeChanged, this,
            &EditorController::multiSelectChanged);

    QUndoStack& stack = m_context.undoStack();
    connect(&stack, &QUndoStack::canUndoChanged, this, &EditorController::undoStateChanged);
    connect(&stack, &QUndoStack::canRedoChanged, this, &EditorController::undoStateChanged);
    connect(&stack, &QUndoStack::indexChanged, this, &EditorController::undoStateChanged);

    // What the editing actions can do depends on the selection, the model
    // (undo and redo change it) and the clipboard.
    connect(&m_context.selection(), &SelectionModel::changed, this,
            &EditorController::editStateChanged);
    connect(&stack, &QUndoStack::indexChanged, this, &EditorController::editStateChanged);
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        connect(clipboard, &QClipboard::dataChanged, this, &EditorController::editStateChanged);

    m_context.setSnapSettings({.toGrid = true, .toObjects = true, .gridSpacing = kGridSpacing});

    DocumentNotifier& notifier = m_context.project().notifier();
    connect(&notifier, &DocumentNotifier::screenRemoved, this, &EditorController::onScreenRemoved);
    connect(&notifier, &DocumentNotifier::screenChanged, this,
            [this](ScreenId changed)
            {
                if (changed == m_context.screenId())
                    Q_EMIT screenChanged();
            });
    connect(&notifier, &DocumentNotifier::screenAdded, this,
            [this]
            {
                // The current screen may have moved; index and
                // canRemoveScreen change.
                if (const auto index = m_context.project().indexOf(m_context.screenId()))
                    m_lastScreenIndex = *index;
                Q_EMIT screenChanged();
            });
    connect(&notifier, &DocumentNotifier::projectReset, this,
            [this]
            {
                const Project& project = m_context.project();
                if (project.screenCount() > 0)
                    showScreen(project.screens().front()->id());
            });
    m_context.setContextMenuRequest([this](QPointF viewPos)
                                    { Q_EMIT contextMenuRequested(viewPos); });
}

EditorController::~EditorController()
{
    m_context.setContextMenuRequest({});
}

void EditorController::attachCanvas(CanvasItem* canvas)
{
    if (m_canvas)
        disconnect(m_canvas, nullptr, this, nullptr);
    m_canvas = canvas;
    m_context.setCanvas(canvas);
    m_toolboxDrag.attachCanvas(canvas);
    if (!canvas)
        return;

    canvas->setInputSettings(m_context.input());
    canvas->setGraphics(m_context.graphics());
    canvas->setScreens(&m_context.project());
    canvas->setGridSpacing(kGridSpacing);
    canvas->setScreen(m_context.screen(), &m_context.project().notifier());
    canvas->setSelection(m_context.selection().ids());

    connect(canvas, &CanvasItem::gesture, m_tools, &ToolController::handleGesture);
    connect(canvas, &CanvasItem::keyPressed, this,
            [this](int key, Qt::KeyboardModifiers modifiers)
            {
                if (!m_tools->keyPress(key, modifiers))
                    handleKey(key, modifiers);
            });
    connect(canvas, &CanvasItem::selectionGeometryChanged, this, &EditorController::updateSizeText);
    connect(canvas, &CanvasItem::viewChanged, this, &EditorController::zoomChanged);
    connect(canvas, &CanvasItem::pointerMoved, this, &EditorController::onPointerMoved);
    connect(&m_context.selection(), &SelectionModel::changed, canvas,
            [this, canvas] { canvas->setSelection(m_context.selection().ids()); });

    m_tools->applyCursor();
    updateSizeText();
    Q_EMIT screenChanged();
    Q_EMIT zoomChanged();
}

void EditorController::showScreen(ScreenId id)
{
    m_context.selection().clear();
    m_context.setScreenId(id);
    if (const auto index = m_context.project().indexOf(id))
        m_lastScreenIndex = *index;
    if (m_canvas)
        m_canvas->setScreen(m_context.screen(), &m_context.project().notifier());
    Q_EMIT currentScreenChanged();
    Q_EMIT screenChanged();
}

void EditorController::onScreenRemoved(ScreenId id)
{
    const Project& project = m_context.project();
    if (id == m_context.screenId() && project.screenCount() > 0)
    {
        // The neighbour that took its place, or the new last screen.
        const std::size_t index = std::min(m_lastScreenIndex, project.screenCount() - 1);
        showScreen(project.screens()[index]->id());
        return;
    }
    if (const auto index = project.indexOf(m_context.screenId()))
        m_lastScreenIndex = *index;
    Q_EMIT screenChanged();
}

void EditorController::setCurrentScreen(int index)
{
    const Project& project = m_context.project();
    if (index < 0 || std::size_t(index) >= project.screenCount())
        return;
    const ScreenId id = project.screens()[std::size_t(index)]->id();
    if (id != m_context.screenId())
        showScreen(id);
}

void EditorController::addScreen()
{
    Project& project = m_context.project();
    const QSizeF size = m_context.screen() ? m_context.screen()->size() : Screen::kDefaultSize;
    // Screen_1, Screen_2, ...
    QString name;
    for (int n = 1;; ++n)
    {
        name = tr("Screen_%1").arg(n);
        const auto& screens = project.screens();
        if (std::ranges::none_of(screens, [&name](const auto& s)
                                 { return s->name().compare(name, Qt::CaseInsensitive) == 0; }))
            break;
    }
    auto screen = std::make_unique<Screen>(name, size);
    const ScreenId id = screen->id();
    m_context.undoStack().push(new AddScreenCommand(project, std::move(screen)));
    showScreen(id);
}

void EditorController::duplicateScreen(int index)
{
    Project& project = m_context.project();
    if (index < 0 || std::size_t(index) >= project.screenCount())
        return;
    const Screen& source = *project.screens()[std::size_t(index)];
    auto copy = source.cloneWithNewIds(tr("%1 copy").arg(source.name()));
    const ScreenId id = copy->id();
    m_context.undoStack().push(new AddScreenCommand(project, std::move(copy), index + 1));
    showScreen(id);
}

void EditorController::removeScreen(int index)
{
    Project& project = m_context.project();
    if (index < 0 || std::size_t(index) >= project.screenCount() || !canRemoveScreen())
        return;
    m_context.undoStack().push(
        new RemoveScreenCommand(project, project.screens()[std::size_t(index)]->id()));
}

void EditorController::renameScreen(int index, const QString& name)
{
    Project& project = m_context.project();
    const QString trimmed = name.trimmed();
    if (index < 0 || std::size_t(index) >= project.screenCount() || trimmed.isEmpty())
        return;
    const Screen& screen = *project.screens()[std::size_t(index)];
    if (screen.name() == trimmed)
        return;
    ScreenSettings settings = ChangeScreenCommand::settingsOf(screen);
    const QString oldName = screen.name();
    settings.name = trimmed;
    QUndoStack& stack = m_context.undoStack();
    stack.beginMacro(tr("Rename screen %1 to %2").arg(oldName, trimmed));
    stack.push(new ChangeScreenCommand(project, screen.id(), settings));
    // Screen windows refer to screens by name; they follow the rename.
    for (const auto& other : project.screens())
    {
        std::vector<ObjectId> windows;
        for (const auto& object : other->objects())
        {
            if (findDescriptor(object->descriptors(), Prop::ScreenName) &&
                object->value<QString>(Prop::ScreenName) == oldName)
                windows.push_back(object->id());
        }
        if (!windows.empty())
            stack.push(new SetPropertyCommand(project, other->id(), std::move(windows),
                                              Prop::ScreenName, trimmed));
    }
    stack.endMacro();
}

int EditorController::currentScreenIndex() const
{
    const auto index = m_context.project().indexOf(m_context.screenId());
    return index ? int(*index) : -1;
}

bool EditorController::canRemoveScreen() const
{
    return m_context.project().screenCount() > 1;
}

void EditorController::selectTool()
{
    m_tools->setTool(ToolController::Tool::Select);
}

void EditorController::panTool()
{
    m_tools->setTool(ToolController::Tool::Pan);
}

void EditorController::createTool(const QString& typeKey)
{
    const ObjectTypeInfo* info = ObjectRegistry::instance().findByKey(typeKey);
    if (!info)
    {
        qWarning("EditorController: unknown object type '%s'", qPrintable(typeKey));
        return;
    }
    m_tools->createObjects(info->id);
}

void EditorController::toggleMultiSelect()
{
    SelectionModel& selection = m_context.selection();
    selection.setMultiSelectMode(!selection.multiSelectMode());
}

void EditorController::zoomIn()
{
    if (m_canvas)
        m_canvas->setZoomCentered(m_canvas->zoom() * kZoomStep);
}

void EditorController::zoomOut()
{
    if (m_canvas)
        m_canvas->setZoomCentered(m_canvas->zoom() / kZoomStep);
}

void EditorController::zoomToFit()
{
    if (m_canvas)
        m_canvas->fitScreen();
}

void EditorController::zoomToActualSize()
{
    if (m_canvas)
        m_canvas->setZoomCentered(1.0);
}

namespace
{
constexpr int kZoomPercents[] = {25, 50, 75, 100, 150, 200, 400};
} // namespace

QStringList EditorController::zoomChoices() const
{
    QStringList choices{tr("Fit")};
    for (const int percent : kZoomPercents)
        choices << tr("%1%").arg(percent);
    return choices;
}

void EditorController::chooseZoom(int index)
{
    if (index == 0)
        zoomToFit();
    else if (index > 0 && index <= int(std::size(kZoomPercents)) && m_canvas)
        m_canvas->setZoomCentered(kZoomPercents[index - 1] / 100.0);
}

double EditorController::zoomLevel() const
{
    return std::log2(m_canvas ? m_canvas->zoom() : 1.0);
}

void EditorController::setZoomLevel(double level)
{
    if (m_canvas && std::abs(level - zoomLevel()) > 1e-6)
        m_canvas->setZoomCentered(std::pow(2.0, level));
}

double EditorController::minimumZoomLevel() const
{
    return std::log2(CanvasItem::kMinZoom);
}

double EditorController::maximumZoomLevel() const
{
    return std::log2(CanvasItem::kMaxZoom);
}

void EditorController::undo()
{
    m_context.undoStack().undo();
}

void EditorController::redo()
{
    m_context.undoStack().redo();
}

void EditorController::selectAll()
{
    m_actions.selectAll();
}

void EditorController::deleteSelection()
{
    m_actions.deleteSelection();
}

void EditorController::duplicate()
{
    m_actions.duplicate();
}

void EditorController::cut()
{
    m_actions.cut();
}

void EditorController::copy()
{
    m_actions.copy();
}

void EditorController::paste()
{
    m_actions.paste();
}

void EditorController::group()
{
    m_actions.group();
}

void EditorController::ungroup()
{
    m_actions.ungroup();
}

void EditorController::nudge(double dx, double dy)
{
    m_actions.nudge({dx, dy});
}

void EditorController::align(Alignment alignment)
{
    static constexpr EditActions::Alignment kMap[] = {
        EditActions::Alignment::Left,           EditActions::Alignment::HorizontalCentre,
        EditActions::Alignment::Right,          EditActions::Alignment::Top,
        EditActions::Alignment::VerticalCentre, EditActions::Alignment::Bottom,
    };
    if (int(alignment) >= 0 && int(alignment) < int(std::size(kMap)))
        m_actions.align(kMap[int(alignment)]);
}

void EditorController::distribute(Qt::Orientation orientation)
{
    m_actions.distribute(orientation);
}

void EditorController::matchSize(SizeMatch match)
{
    static constexpr EditActions::SizeMatch kMap[] = {
        EditActions::SizeMatch::Width,
        EditActions::SizeMatch::Height,
        EditActions::SizeMatch::Both,
    };
    if (int(match) >= 0 && int(match) < int(std::size(kMap)))
        m_actions.matchSize(kMap[int(match)]);
}

void EditorController::reorder(Order order)
{
    static constexpr EditActions::Order kMap[] = {
        EditActions::Order::Forward,
        EditActions::Order::Backward,
        EditActions::Order::ToFront,
        EditActions::Order::ToBack,
    };
    if (int(order) >= 0 && int(order) < int(std::size(kMap)))
        m_actions.reorder(kMap[int(order)]);
}

void EditorController::applyProjectStyle()
{
    m_actions.applyProjectStyle();
}

bool EditorController::handleKey(int key, Qt::KeyboardModifiers modifiers)
{
    const double step = modifiers.testFlag(Qt::ShiftModifier) ? kLargeNudge : 1.0;
    switch (key)
    {
    case Qt::Key_Left:
        return m_actions.nudge({-step, 0});
    case Qt::Key_Right:
        return m_actions.nudge({step, 0});
    case Qt::Key_Up:
        return m_actions.nudge({0, -step});
    case Qt::Key_Down:
        return m_actions.nudge({0, step});
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        return m_actions.deleteSelection();
    default:
        return false;
    }
}

void EditorController::updateSizeText()
{
    QString text;
    if (m_canvas && !m_canvas->selectionBounds().isEmpty())
    {
        const QSizeF size = m_canvas->selectionSceneSize();
        text = tr("%1 × %2").arg(std::lround(size.width())).arg(std::lround(size.height()));
        if (const double rotation = m_canvas->selectionRotation(); rotation != 0.0)
            text = tr("%1 · %2°").arg(text).arg(rotation);
    }
    if (text == m_sizeText)
        return;
    m_sizeText = text;
    Q_EMIT sizeTextChanged();
}

void EditorController::onPointerMoved(QPointF scenePos)
{
    m_cursor = scenePos;
    Q_EMIT cursorChanged();
}

QString EditorController::projectTitle() const
{
    return m_context.project().name();
}

QString EditorController::screenTitle() const
{
    const Screen* screen = m_context.screen();
    return screen ? screen->name() : QString();
}

QString EditorController::zoomText() const
{
    const double zoom = m_canvas ? m_canvas->zoom() : 1.0;
    return tr("%1%").arg(std::lround(zoom * 100.0));
}

QString EditorController::gridText() const
{
    return tr("Grid %1 px").arg(kGridSpacing);
}

QString EditorController::cursorText() const
{
    return tr("X %1 · Y %2").arg(std::lround(m_cursor.x())).arg(std::lround(m_cursor.y()));
}

QString EditorController::selectionText() const
{
    const auto count = int(m_context.selection().size());
    return count == 0 ? tr("No selection") : tr("%n selected", nullptr, count);
}

bool EditorController::selectToolActive() const
{
    return m_tools->tool() == ToolController::Tool::Select;
}

bool EditorController::panToolActive() const
{
    return m_tools->tool() == ToolController::Tool::Pan;
}

bool EditorController::createToolActive() const
{
    return m_tools->tool() == ToolController::Tool::Create ||
           m_tools->tool() == ToolController::Tool::Path;
}

QString EditorController::createType() const
{
    if (!createToolActive())
        return {};
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(m_tools->createType());
    return info ? info->key : QString();
}

bool EditorController::multiSelect() const
{
    return m_context.selection().multiSelectMode();
}

bool EditorController::canUndo() const
{
    return m_context.undoStack().canUndo();
}

bool EditorController::canRedo() const
{
    return m_context.undoStack().canRedo();
}

QString EditorController::undoText() const
{
    const QString text = m_context.undoStack().undoText();
    return text.isEmpty() ? tr("Undo") : tr("Undo %1").arg(text);
}

QString EditorController::redoText() const
{
    const QString text = m_context.undoStack().redoText();
    return text.isEmpty() ? tr("Redo") : tr("Redo %1").arg(text);
}

bool EditorController::hasSelection() const
{
    return m_actions.hasSelection();
}

bool EditorController::canGroup() const
{
    return m_actions.canGroup();
}

bool EditorController::canUngroup() const
{
    return m_actions.canUngroup();
}

bool EditorController::canPaste() const
{
    return m_actions.canPaste();
}

bool EditorController::canAlign() const
{
    return m_actions.canAlign();
}

bool EditorController::canDistribute() const
{
    return m_actions.canDistribute();
}

bool EditorController::canMatchSize() const
{
    return m_actions.canMatchSize();
}

bool EditorController::snapEnabled() const
{
    return m_context.snapEnabled();
}

void EditorController::setSnapEnabled(bool enabled)
{
    if (enabled == m_context.snapEnabled())
        return;
    m_context.setSnapEnabled(enabled);
    Q_EMIT snapEnabledChanged();
}

} // namespace hmi
