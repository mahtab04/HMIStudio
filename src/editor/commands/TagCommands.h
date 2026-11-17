#pragma once

#include "core/model/Events.h"
#include "core/model/Ids.h"
#include "core/tags/Binding.h"
#include "core/tags/Connection.h"
#include "core/tags/Tag.h"

#include <QUndoCommand>

#include <optional>
#include <vector>

namespace hmi
{

class Project;

// Replaces the small connection collection as one atomic undo step.
class ChangeConnectionsCommand : public QUndoCommand
{
public:
    ChangeConnectionsCommand(Project& project, std::vector<Connection> after);
    void redo() override;
    void undo() override;

private:
    Project& m_project;
    std::vector<Connection> m_before;
    std::vector<Connection> m_after;
};

// Adds a tag to the project's TagDatabase.
class AddTagCommand : public QUndoCommand
{
public:
    AddTagCommand(Project& project, Tag tag, std::optional<std::size_t> index = std::nullopt,
                  QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    Project& m_project;
    Tag m_tag;
    std::optional<std::size_t> m_index;
};

// Removes a tag. Bindings that use it stay and show the tag as missing.
class RemoveTagCommand : public QUndoCommand
{
public:
    RemoveTagCommand(Project& project, const QString& name, QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    Project& m_project;
    QString m_name;
    Tag m_tag;
    std::size_t m_index = 0;
};

// Replaces a tag's settings, possibly renaming it. The old settings are read
// when the command is created.
class ChangeTagCommand : public QUndoCommand
{
public:
    ChangeTagCommand(Project& project, const QString& name, Tag after,
                     QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    Project& m_project;
    Tag m_before;
    Tag m_after;
    struct Reference
    {
        ScreenId screen;
        ObjectId root;
        ObjectId object;
        PropertyId property;
        std::optional<Binding> binding;
        QString spelling;
    };
    std::vector<Reference> m_references;
    // Event action lists naming the tag, before the rename.
    struct EventReference
    {
        ScreenId screen;
        ObjectId root;
        ObjectId object;
        EventKind event;
        ActionList before;
    };
    std::vector<EventReference> m_eventReferences;
    void applyReferences(bool forward);
};

// Sets (or, with no binding, removes) the binding of one property on objects
// of a screen. The old bindings are read when the command is created.
class SetBindingCommand : public QUndoCommand
{
public:
    SetBindingCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                      PropertyId property, std::optional<Binding> binding,
                      QUndoCommand* parent = nullptr);
    void redo() override;
    void undo() override;

private:
    struct Change
    {
        ObjectId object;
        std::optional<Binding> before;
    };
    void apply(ObjectId object, const std::optional<Binding>& binding);

    Project& m_project;
    ScreenId m_screen;
    PropertyId m_property;
    std::optional<Binding> m_after;
    std::vector<Change> m_changes;
};

} // namespace hmi
