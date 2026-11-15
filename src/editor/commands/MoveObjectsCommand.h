#pragma once

#include "core/model/Ids.h"

#include <QPointF>
#include <QUndoCommand>

#include <vector>

namespace hmi
{

class Project;

// Moves objects of one screen by the same scene offset. Nudges (arrow keys
// and the touch toolbar's arrow buttons) merge: consecutive nudges of the
// same objects become one undo step.
class MoveObjectsCommand : public QUndoCommand
{
public:
    enum class Kind
    {
        Drag,
        Nudge,
    };

    MoveObjectsCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                       QPointF delta, Kind kind = Kind::Drag, QUndoCommand* parent = nullptr);

    [[nodiscard]] const std::vector<ObjectId>& objects() const noexcept { return m_objects; }
    [[nodiscard]] QPointF delta() const noexcept { return m_delta; }

    void redo() override;
    void undo() override;
    [[nodiscard]] int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    void apply(QPointF delta);

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_objects;
    QPointF m_delta;
    Kind m_kind;
};

} // namespace hmi
