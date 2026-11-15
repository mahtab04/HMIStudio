#include "editor/commands/MoveObjectsCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"

#include <QCoreApplication>

namespace hmi
{

MoveObjectsCommand::MoveObjectsCommand(Project& project, ScreenId screen,
                                       std::vector<ObjectId> objects, QPointF delta, Kind kind,
                                       QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_objects(std::move(objects))
    , m_delta(delta)
    , m_kind(kind)
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

namespace
{
// QUndoStack merges only commands with the same id; -1 means never.
constexpr int kNudgeId = 0x4E554447; // "NUDG"
} // namespace

int MoveObjectsCommand::id() const
{
    return m_kind == Kind::Nudge ? kNudgeId : -1;
}

bool MoveObjectsCommand::mergeWith(const QUndoCommand* other)
{
    const auto* move = dynamic_cast<const MoveObjectsCommand*>(other);
    if (!move || move->m_kind != Kind::Nudge || move->m_screen != m_screen ||
        move->m_objects != m_objects)
        return false;
    m_delta += move->m_delta;
    // Nudging back to the start leaves nothing to undo.
    setObsolete(m_delta.isNull());
    return true;
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
