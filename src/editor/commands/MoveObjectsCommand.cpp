#include "editor/commands/MoveObjectsCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"

#include <QCoreApplication>

namespace hmi
{

MoveObjectsCommand::MoveObjectsCommand(Project& project, ScreenId screen,
                                       std::vector<ObjectId> objects, QPointF delta,
                                       QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_objects(std::move(objects))
    , m_delta(delta)
{
    setText(QCoreApplication::translate("hmi::Commands", "Move %n object(s)", nullptr,
                                        int(m_objects.size())));
}

void MoveObjectsCommand::redo()
{
    apply(m_delta);
}

void MoveObjectsCommand::undo()
{
    apply(-m_delta);
}

void MoveObjectsCommand::apply(QPointF delta)
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    for (const ObjectId& id : m_objects)
    {
        HmiObject* object = screen->object(id);
        if (!object)
            continue;
        const QPointF pos = object->geometry().topLeft() + delta;
        [[maybe_unused]] const auto x = object->setProperty(Prop::X, pos.x());
        [[maybe_unused]] const auto y = object->setProperty(Prop::Y, pos.y());
        Q_ASSERT(x && y);
    }
}

} // namespace hmi
