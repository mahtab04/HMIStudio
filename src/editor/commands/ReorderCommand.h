#pragma once

#include "core/model/Ids.h"

#include <QUndoCommand>

#include <vector>

namespace hmi
{

class Project;

// Changes the z-order of a screen's objects (bring forward, send to back and
// so on). Holds the complete order before and after, bottom first.
class ReorderCommand : public QUndoCommand
{
public:
    ReorderCommand(Project& project, ScreenId screen, std::vector<ObjectId> before,
                   std::vector<ObjectId> after, const QString& text,
                   QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    void apply(const std::vector<ObjectId>& order);

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_before;
    std::vector<ObjectId> m_after;
};

} // namespace hmi
