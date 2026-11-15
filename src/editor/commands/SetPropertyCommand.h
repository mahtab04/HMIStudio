#pragma once

#include "core/model/Ids.h"
#include "core/property/PropertyValue.h"

#include <QUndoCommand>

#include <vector>

namespace hmi
{

class Project;

// Sets one property to the same value on one or more objects of a screen.
// The old values are read from the model when the command is created.
// Objects that do not have the property are skipped.
class SetPropertyCommand : public QUndoCommand
{
public:
    SetPropertyCommand(Project& project, ScreenId screen, std::vector<ObjectId> objects,
                       PropertyId property, PropertyValue value, QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    struct Change
    {
        ObjectId object;
        PropertyValue before;
    };

    Project& m_project;
    ScreenId m_screen;
    PropertyId m_property;
    PropertyValue m_value;
    std::vector<Change> m_changes;
};

} // namespace hmi
