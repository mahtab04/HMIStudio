#include "editor/commands/ResizeObjectCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"

#include <QCoreApplication>

namespace hmi
{

ResizeObjectCommand::ResizeObjectCommand(Project& project, ScreenId screen, ObjectId object,
                                         const QRectF& before, const QRectF& after,
                                         QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_object(object)
    , m_before(before)
    , m_after(after)
{
    setText(QCoreApplication::translate("hmi::Commands", "Resize object"));
}

void ResizeObjectCommand::redo()
{
    apply(m_after);
}

void ResizeObjectCommand::undo()
{
    apply(m_before);
}

void ResizeObjectCommand::apply(const QRectF& geometry)
{
    Screen* screen = m_project.screen(m_screen);
    HmiObject* object = screen ? screen->object(m_object) : nullptr;
    if (!object)
        return;
    [[maybe_unused]] const bool ok = setGeometry(*object, geometry);
    Q_ASSERT_X(ok, "ResizeObjectCommand", "geometry rejected");
}

bool ResizeObjectCommand::setGeometry(HmiObject& object, const QRectF& geometry)
{
    const bool x = object.setProperty(Prop::X, geometry.x()).has_value();
    const bool y = object.setProperty(Prop::Y, geometry.y()).has_value();
    const bool w = object.setProperty(Prop::Width, geometry.width()).has_value();
    const bool h = object.setProperty(Prop::Height, geometry.height()).has_value();
    return x && y && w && h;
}

} // namespace hmi
