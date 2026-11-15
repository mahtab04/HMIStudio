#pragma once

#include "core/model/Ids.h"

#include <QUndoCommand>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;
class Project;

// Replaces groups with their children, converted to screen coordinates
// (GroupObject::ungroupedChildren()). The children take the group's z-index
// and keep their ids. Objects in the list that have no children are left
// alone. While done, the command owns the groups.
class UngroupCommand : public QUndoCommand
{
public:
    UngroupCommand(Project& project, ScreenId screen, std::vector<ObjectId> groups,
                   QUndoCommand* parent = nullptr);
    ~UngroupCommand() override;
    UngroupCommand(const UngroupCommand&) = delete;
    UngroupCommand& operator=(const UngroupCommand&) = delete;
    UngroupCommand(UngroupCommand&&) = delete;
    UngroupCommand& operator=(UngroupCommand&&) = delete;

    // Ids of the released children, in z-order; filled by redo().
    [[nodiscard]] const std::vector<ObjectId>& childIds() const noexcept { return m_childIds; }

    void redo() override;
    void undo() override;

private:
    struct Ungrouped
    {
        std::size_t index = 0;
        std::unique_ptr<HmiObject> group;
        std::vector<ObjectId> children;
    };

    Project& m_project;
    ScreenId m_screen;
    std::vector<ObjectId> m_groups;
    std::vector<Ungrouped> m_ungrouped; // ascending by original index
    std::vector<ObjectId> m_childIds;
};

} // namespace hmi
