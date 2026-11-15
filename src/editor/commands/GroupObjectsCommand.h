#pragma once

#include "core/model/Ids.h"

#include <QUndoCommand>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;
class Project;

// Replaces objects with one group holding them. The group takes the z-index
// of the topmost grouped object; the children keep their relative order.
// While done, the command owns the original objects; each redo builds the
// group again from them with the same group id, so the model is identical
// every time.
class GroupObjectsCommand : public QUndoCommand
{
public:
    GroupObjectsCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                        QUndoCommand* parent = nullptr);
    ~GroupObjectsCommand() override;
    GroupObjectsCommand(const GroupObjectsCommand&) = delete;
    GroupObjectsCommand& operator=(const GroupObjectsCommand&) = delete;
    GroupObjectsCommand(GroupObjectsCommand&&) = delete;
    GroupObjectsCommand& operator=(GroupObjectsCommand&&) = delete;

    [[nodiscard]] ObjectId groupId() const noexcept { return m_groupId; }

    void redo() override;
    void undo() override;

private:
    struct Original
    {
        std::size_t index = 0;
        std::unique_ptr<HmiObject> object;
    };

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_objects;
    ObjectId m_groupId = ObjectId::create();
    std::vector<Original> m_originals; // ascending by index
};

} // namespace hmi
