#include "editor/commands/AddObjectCommand.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>

namespace hmi
{

AddObjectCommand::AddObjectCommand(Project& project, ScreenId screen,
                                   std::unique_ptr<HmiObject> object,
                                   std::optional<std::size_t> index, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_project(project)
    , m_screen(screen)
    , m_object(std::move(object))
    , m_index(index)
{
    Q_ASSERT(m_object);
    m_objectId = m_object->id();
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(m_object->typeId());
    const QString typeName =
        info ? QCoreApplication::translate("hmi::ObjectRegistry", info->displayName) : QString();
    setText(QCoreApplication::translate("hmi::Commands", "Add %1").arg(typeName));
}

AddObjectCommand::~AddObjectCommand() = default;

void AddObjectCommand::redo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen || !m_object)
    {
        qWarning("AddObjectCommand: screen or object missing");
        return;
    }
    auto inserted = screen->insertObject(std::move(m_object), m_index);
    if (!inserted)
    {
        qWarning("AddObjectCommand: %s", qPrintable(inserted.error().message));
        return;
    }
    // Undo followed by redo puts the object back at the same z-index.
    m_index = screen->indexOf(m_objectId);
}

void AddObjectCommand::undo()
{
    Screen* screen = m_project.screen(m_screen);
    if (!screen)
        return;
    auto taken = screen->takeObject(m_objectId);
    if (!taken)
    {
        qWarning("AddObjectCommand: %s", qPrintable(taken.error().message));
        return;
    }
    m_object = std::move(*taken);
}

} // namespace hmi
