#pragma once

#include "core/model/Ids.h"
#include "quick/CanvasItem.h"

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <qqmlintegration.h>

class QUndoStack;

namespace hmi
{

class SelectionModel;
class ToolContext;
class ToolController;
class ToolboxDragController;

// The single object the QML shell talks to for editing (Facade): tool
// switching, zoom, undo and redo, plus display state for the toolbar and
// status bar. Text shown in QML is formatted here so QML only binds.
class EditorController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("EditorController is provided by the application")

    Q_PROPERTY(QString projectTitle READ projectTitle CONSTANT)
    Q_PROPERTY(QString screenTitle READ screenTitle NOTIFY screenChanged)
    Q_PROPERTY(QString zoomText READ zoomText NOTIFY zoomChanged)
    Q_PROPERTY(QString gridText READ gridText CONSTANT)
    Q_PROPERTY(QString cursorText READ cursorText NOTIFY cursorChanged)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY selectionChanged)
    Q_PROPERTY(bool selectToolActive READ selectToolActive NOTIFY toolChanged)
    Q_PROPERTY(bool panToolActive READ panToolActive NOTIFY toolChanged)
    Q_PROPERTY(bool createToolActive READ createToolActive NOTIFY toolChanged)
    Q_PROPERTY(QString createType READ createType NOTIFY toolChanged)
    Q_PROPERTY(bool multiSelect READ multiSelect NOTIFY multiSelectChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)

public:
    static constexpr double kZoomStep = 1.25;
    static constexpr double kGridSpacing = 8.0;

    EditorController(ToolContext& context, ToolboxDragController& toolboxDrag,
                     QObject* parent = nullptr);
    ~EditorController() override;
    EditorController(const EditorController&) = delete;
    EditorController& operator=(const EditorController&) = delete;
    EditorController(EditorController&&) = delete;
    EditorController& operator=(EditorController&&) = delete;

    // Connects the canvas created by QML: shows the current screen, receives
    // its gestures and keys, mirrors the selection.
    Q_INVOKABLE void attachCanvas(hmi::CanvasItem* canvas);

    Q_INVOKABLE void selectTool();
    Q_INVOKABLE void panTool();
    Q_INVOKABLE void createTool(const QString& typeKey);
    Q_INVOKABLE void toggleMultiSelect();

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();
    Q_INVOKABLE void zoomToActualSize();

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    [[nodiscard]] QString projectTitle() const;
    [[nodiscard]] QString screenTitle() const;
    [[nodiscard]] QString zoomText() const;
    [[nodiscard]] QString gridText() const;
    [[nodiscard]] QString cursorText() const;
    [[nodiscard]] QString selectionText() const;
    [[nodiscard]] bool selectToolActive() const;
    [[nodiscard]] bool panToolActive() const;
    [[nodiscard]] bool createToolActive() const;
    [[nodiscard]] QString createType() const;
    [[nodiscard]] bool multiSelect() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;

    [[nodiscard]] ToolController& tools() const noexcept { return *m_tools; }

Q_SIGNALS:
    void screenChanged();
    void zoomChanged();
    void cursorChanged();
    void selectionChanged();
    void toolChanged();
    void multiSelectChanged();
    void undoStateChanged();

private:
    void onPointerMoved(QPointF scenePos);

    ToolContext& m_context;
    ToolboxDragController& m_toolboxDrag;
    ToolController* m_tools = nullptr;
    QPointer<CanvasItem> m_canvas;
    QPointF m_cursor;
};

} // namespace hmi
