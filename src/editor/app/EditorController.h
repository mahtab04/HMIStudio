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

class EditActions;
class SelectionModel;
class ToolContext;
class ToolController;
class ToolboxDragController;

// The single object the QML shell talks to for editing (Facade): tool
// switching, zoom, undo and redo, the editing actions (EditActions), plus
// display state for the toolbar, context menu, selection toolbar and status
// bar. Text shown in QML is formatted here so QML only binds.
class EditorController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("EditorController is provided by the application")

    Q_PROPERTY(QString projectTitle READ projectTitle CONSTANT)
    Q_PROPERTY(QString screenTitle READ screenTitle NOTIFY screenChanged)
    Q_PROPERTY(int currentScreenIndex READ currentScreenIndex NOTIFY screenChanged)
    Q_PROPERTY(bool canRemoveScreen READ canRemoveScreen NOTIFY screenChanged)
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
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY editStateChanged)
    Q_PROPERTY(bool canGroup READ canGroup NOTIFY editStateChanged)
    Q_PROPERTY(bool canUngroup READ canUngroup NOTIFY editStateChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY editStateChanged)
    Q_PROPERTY(bool canAlign READ canAlign NOTIFY editStateChanged)
    Q_PROPERTY(bool canDistribute READ canDistribute NOTIFY editStateChanged)
    Q_PROPERTY(bool canMatchSize READ canMatchSize NOTIFY editStateChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY sizeTextChanged)

public:
    enum Alignment
    {
        AlignLeft,
        AlignHorizontalCentre,
        AlignRight,
        AlignTop,
        AlignVerticalCentre,
        AlignBottom,
    };
    Q_ENUM(Alignment)

    enum Order
    {
        BringForward,
        SendBackward,
        BringToFront,
        SendToBack,
    };
    Q_ENUM(Order)

    enum SizeMatch
    {
        SameWidth,
        SameHeight,
        SameSize,
    };
    Q_ENUM(SizeMatch)

    // Arrow keys move by one unit, with Shift by kLargeNudge.
    static constexpr double kLargeNudge = 10.0;

    static constexpr double kZoomStep = 1.25;
    static constexpr double kGridSpacing = 8.0;

    EditorController(ToolContext& context, EditActions& actions, ToolboxDragController& toolboxDrag,
                     QObject* parent = nullptr);
    ~EditorController() override;
    EditorController(const EditorController&) = delete;
    EditorController& operator=(const EditorController&) = delete;
    EditorController(EditorController&&) = delete;
    EditorController& operator=(EditorController&&) = delete;

    // Connects the canvas created by QML: shows the current screen, receives
    // its gestures and keys, mirrors the selection.
    Q_INVOKABLE void attachCanvas(hmi::CanvasItem* canvas);

    // Screens: switch, add, duplicate, delete, rename. Index is the
    // screen's position in the project.
    Q_INVOKABLE void setCurrentScreen(int index);
    Q_INVOKABLE void addScreen();
    Q_INVOKABLE void duplicateScreen(int index);
    Q_INVOKABLE void removeScreen(int index);
    Q_INVOKABLE void renameScreen(int index, const QString& name);

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

    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void duplicate();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void group();
    Q_INVOKABLE void ungroup();
    Q_INVOKABLE void nudge(double dx, double dy);
    Q_INVOKABLE void align(hmi::EditorController::Alignment alignment);
    Q_INVOKABLE void distribute(Qt::Orientation orientation);
    Q_INVOKABLE void matchSize(hmi::EditorController::SizeMatch match);
    Q_INVOKABLE void reorder(hmi::EditorController::Order order);

    // Keys the canvas tools did not use: arrows nudge, Delete removes.
    bool handleKey(int key, Qt::KeyboardModifiers modifiers);

    [[nodiscard]] QString projectTitle() const;
    [[nodiscard]] QString screenTitle() const;
    [[nodiscard]] int currentScreenIndex() const;
    [[nodiscard]] bool canRemoveScreen() const;
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
    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] bool canGroup() const;
    [[nodiscard]] bool canUngroup() const;
    [[nodiscard]] bool canPaste() const;
    [[nodiscard]] bool canAlign() const;
    [[nodiscard]] bool canDistribute() const;
    [[nodiscard]] bool canMatchSize() const;
    [[nodiscard]] bool snapEnabled() const;
    void setSnapEnabled(bool enabled);
    [[nodiscard]] QString sizeText() const { return m_sizeText; }

    [[nodiscard]] ToolController& tools() const noexcept { return *m_tools; }

Q_SIGNALS:
    void screenChanged();
    // Another screen is now shown (switch, open, delete of the current one).
    void currentScreenChanged();
    void zoomChanged();
    void cursorChanged();
    void selectionChanged();
    void toolChanged();
    void multiSelectChanged();
    void undoStateChanged();
    void editStateChanged();
    void snapEnabledChanged();
    void sizeTextChanged();
    // Right-click on the canvas; `viewPos` is in canvas coordinates.
    void contextMenuRequested(QPointF viewPos);

private:
    void onPointerMoved(QPointF scenePos);
    // Shows the screen on the canvas and makes the tools work on it.
    void showScreen(ScreenId id);
    void onScreenRemoved(ScreenId id);
    void updateSizeText();

    ToolContext& m_context;
    EditActions& m_actions;
    ToolboxDragController& m_toolboxDrag;
    ToolController* m_tools = nullptr;
    QPointer<CanvasItem> m_canvas;
    QPointF m_cursor;
    std::size_t m_lastScreenIndex = 0;
    QString m_sizeText;
};

} // namespace hmi
