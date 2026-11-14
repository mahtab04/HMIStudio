#pragma once

#include "core/model/Ids.h"

#include <QRectF>
#include <QUndoCommand>

namespace hmi
{

class HmiObject;
class Project;

// Sets one object's geometry (position and size) from `before` to `after`.
class ResizeObjectCommand : public QUndoCommand
{
public:
    ResizeObjectCommand(Project& project, ScreenId screen, ObjectId object, const QRectF& before,
                        const QRectF& after, QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

    // Writes x, y, width and height. Returns false if a value was rejected.
    static bool setGeometry(HmiObject& object, const QRectF& geometry);

private:
    void apply(const QRectF& geometry);

    Project& m_project;
    ScreenId m_screen;
    ObjectId m_object;
    QRectF m_before;
    QRectF m_after;
};

} // namespace hmi
