#pragma once

#include "core/model/Ids.h"

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <qqmlintegration.h>

namespace hmi
{

class CanvasItem;
class ToolContext;

// Drag and drop from the toolbox onto the canvas, for mouse and touch alike.
// A toolbox tile's DragHandler reports start, end and window positions; this
// controller shows where the object will land and adds it on release over
// the canvas. Releasing elsewhere does nothing.
class ToolboxDragController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ToolboxDragController is provided by the application")
    Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged)

public:
    explicit ToolboxDragController(ToolContext& context, QObject* parent = nullptr);

    void attachCanvas(CanvasItem* canvas);

    [[nodiscard]] bool dragging() const noexcept { return !m_type.isNull(); }

    // `active` true starts a drag of type `key`; false drops it.
    Q_INVOKABLE void setActive(bool active, const QString& key);
    // Pointer position in window (QML scene) coordinates.
    Q_INVOKABLE void move(QPointF windowPos);

Q_SIGNALS:
    void draggingChanged();

private:
    void drop();

    ToolContext& m_context;
    QPointer<CanvasItem> m_canvas;
    ObjectTypeId m_type;
    QPointF m_scenePos;
    bool m_overCanvas = false;
};

} // namespace hmi
