#include "editor/tools/ObjectPlacement.h"

#include "core/model/HmiObject.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"
#include "editor/commands/AddObjectCommand.h"
#include "editor/commands/ResizeObjectCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QUndoStack>

#include <cmath>

namespace hmi::ObjectPlacement
{

namespace
{

QRectF rounded(const QRectF& rect)
{
    const QRectF r = rect.normalized();
    return {std::round(r.x()), std::round(r.y()), std::round(r.width()), std::round(r.height())};
}

} // namespace

QSizeF defaultSize(ObjectTypeId type)
{
    const auto object = ObjectRegistry::instance().create(type);
    return object ? object->geometry().size() : QSizeF();
}

std::unique_ptr<HmiObject> createCentred(ObjectTypeId type, QPointF sceneCentre)
{
    auto object = ObjectRegistry::instance().create(type);
    if (!object)
        return nullptr;
    const QSizeF size = object->geometry().size();
    const QRectF rect(sceneCentre - QPointF(size.width() / 2.0, size.height() / 2.0), size);
    if (!ResizeObjectCommand::setGeometry(*object, rounded(rect)))
        return nullptr;
    return object;
}

std::unique_ptr<HmiObject> createInRect(ObjectTypeId type, const QRectF& sceneRect, bool rising)
{
    auto object = ObjectRegistry::instance().create(type);
    if (!object || !ResizeObjectCommand::setGeometry(*object, rounded(sceneRect)))
        return nullptr;
    // Types with a line direction take it from the drag; the value comes from
    // the descriptor's option keys, not from the concrete class.
    if (const PropertyDescriptor* d = findDescriptor(object->descriptors(), Prop::LineDirection))
    {
        const std::string_view wanted = rising ? "rising" : "falling";
        for (const EnumOption& option : d->constraint.enumOptions)
        {
            if (wanted == option.key)
            {
                [[maybe_unused]] const auto ok =
                    object->setProperty(Prop::LineDirection, EnumValue{option.value});
            }
        }
    }
    return object;
}

std::optional<ObjectId> add(ToolContext& context, std::unique_ptr<HmiObject> object)
{
    if (!object || !context.screen())
        return std::nullopt;
    const ObjectId id = object->id();
    context.undoStack().push(
        new AddObjectCommand(context.project(), context.screenId(), std::move(object)));
    context.selection().selectOnly(id);
    return id;
}

} // namespace hmi::ObjectPlacement
