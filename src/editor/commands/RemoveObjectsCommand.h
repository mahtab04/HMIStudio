#pragma once

#include "core/model/Ids.h"

#include <QUndoCommand>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;
class Project;

// Removes objects from a screen. While done, the command owns the removed
// objects and remembers their z-indices, so undo puts each back in place.
class RemoveObjectsCommand : public QUndoCommand
{
public:
    RemoveObjectsCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                         QUndoCommand* parent = nullptr);
    ~RemoveObjectsCommand() override;
    RemoveObjectsCommand(const RemoveObjectsCommand&) = delete;
    RemoveObjectsCommand& operator=(const RemoveObjectsCommand&) = delete;
    RemoveObjectsCommand(RemoveObjectsCommand&&) = delete;
    RemoveObjectsCommand& operator=(RemoveObjectsCommand&&) = delete;

    void redo() override;
    void undo() override;

private:
    struct Removed
    {
        std::size_t index = 0;
        std::unique_ptr<HmiObject> object;
    };

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_objects;
    std::vector<Removed> m_removed; // ascending by index
};

} // namespace hmi
