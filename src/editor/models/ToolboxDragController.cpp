#include "editor/models/ToolboxDragController.h"

#include "editor/tools/ObjectPlacement.h"
#include "editor/tools/ToolContext.h"
#include "quick/CanvasItem.h"

namespace hmi
{

ToolboxDragController::ToolboxDragController(ToolContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{
}

void ToolboxDragController::attachCanvas(CanvasItem* canvas)
{
    m_canvas = canvas;
}

void ToolboxDragController::setActive(bool active, const QString& key)
{
    if (active)
    {
        m_type = ObjectTypeId::fromKey(key);
        m_overCanvas = false;
        Q_EMIT draggingChanged();
        return;
    }
    if (m_type.isNull())
        return;
    drop();
    m_type = {};
    Q_EMIT draggingChanged();
}

void ToolboxDragController::move(QPointF windowPos)
{
    if (m_type.isNull() || !m_canvas)
        return;
    const QPointF viewPos = m_canvas->mapFromScene(windowPos);
    m_overCanvas = m_canvas->contains(viewPos);
    if (!m_overCanvas)
    {
        m_canvas->setRubberBand(std::nullopt);
        return;
    }
    m_scenePos = m_canvas->mapViewToScene(viewPos);
    const QSizeF size = ObjectPlacement::defaultSize(m_type);
    m_canvas->setRubberBand(
        QRectF(m_scenePos - QPointF(size.width() / 2.0, size.height() / 2.0), size));
}

void ToolboxDragController::drop()
{
    if (m_canvas)
        m_canvas->setRubberBand(std::nullopt);
    if (!m_overCanvas)
        return;
    ObjectPlacement::add(m_context, ObjectPlacement::createCentred(m_type, m_scenePos));
}

} // namespace hmi
