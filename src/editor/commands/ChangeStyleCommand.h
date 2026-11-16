#pragma once

#include "core/model/HmiStyle.h"

#include <QUndoCommand>

namespace hmi
{

class Project;

// Changes the project style (HmiStyle). Objects already placed keep their
// look until "Apply project style"; new objects take the new style.
class ChangeStyleCommand : public QUndoCommand
{
public:
    ChangeStyleCommand(Project& project, const HmiStyle& after, QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    Project& m_project;
    HmiStyle m_before;
    HmiStyle m_after;
};

} // namespace hmi
