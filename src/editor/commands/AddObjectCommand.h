#pragma once

#include "core/model/Ids.h"

#include <QUndoCommand>

#include <memory>
#include <optional>

namespace hmi
{

class HmiObject;
class Project;

// Adds one object to a screen. While undone, the command owns the object.
class AddObjectCommand : public QUndoCommand
{
public:
    // `index` is the z-index to insert at; default on top.
    AddObjectCommand(Project& project, ScreenId screen, std::unique_ptr<HmiObject> object,
                     std::optional<std::size_t> index = std::nullopt,
                     QUndoCommand* parent = nullptr);
    ~AddObjectCommand() override;
    AddObjectCommand(const AddObjectCommand&) = delete;
    AddObjectCommand& operator=(const AddObjectCommand&) = delete;
    AddObjectCommand(AddObjectCommand&&) = delete;
    AddObjectCommand& operator=(AddObjectCommand&&) = delete;

    [[nodiscard]] ObjectId objectId() const noexcept { return m_objectId; }

    void redo() override;
    void undo() override;

private:
    Project& m_project;
    ScreenId m_screen;
    ObjectId m_objectId;
    std::unique_ptr<HmiObject> m_object;
    std::optional<std::size_t> m_index;
};

} // namespace hmi
