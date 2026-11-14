#pragma once

#include "core/model/Ids.h"

#include <QPointF>
#include <QUndoCommand>

#include <vector>

namespace hmi
{

class Project;

// Moves objects of one screen by the same scene offset.
class MoveObjectsCommand : public QUndoCommand
{
public:
    MoveObjectsCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                       QPointF delta, QUndoCommand* parent = nullptr);

    [[nodiscard]] const std::vector<ObjectId>& objects() const noexcept { return m_objects; }
    [[nodiscard]] QPointF delta() const noexcept { return m_delta; }

    void redo() override;
    void undo() override;

private:
    void apply(QPointF delta);

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_objects;
    QPointF m_delta;
};

} // namespace hmi
