#include "editor/app/EditorController.h"

#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/tools/ToolContext.h"
#include "editor/tools/ToolController.h"
#include "quick/CanvasItem.h"

#include <QUndoStack>

#include <cmath>

namespace hmi
{

EditorController::EditorController(ToolContext& context, ToolboxDragController& toolboxDrag,
                                   QObject* parent)
    : QObject(parent)
    , m_context(context)
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
}

EditorController::~EditorController() = default;

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
    canvas->setGridSpacing(kGridSpacing);
    canvas->setScreen(m_context.screen(), &m_context.project().notifier());
    canvas->setSelection(m_context.selection().ids());

    connect(canvas, &CanvasItem::gesture, m_tools, &ToolController::handleGesture);
    connect(canvas, &CanvasItem::keyPressed, m_tools, &ToolController::keyPress);
    connect(canvas, &CanvasItem::viewChanged, this, &EditorController::zoomChanged);
    connect(canvas, &CanvasItem::pointerMoved, this, &EditorController::onPointerMoved);
    connect(&m_context.selection(), &SelectionModel::changed, canvas,
            [this, canvas] { canvas->setSelection(m_context.selection().ids()); });

    m_tools->applyCursor();
    Q_EMIT screenChanged();
    Q_EMIT zoomChanged();
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

void EditorController::undo()
{
    m_context.undoStack().undo();
}

void EditorController::redo()
{
    m_context.undoStack().redo();
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
    return m_tools->tool() == ToolController::Tool::Create;
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

} // namespace hmi
