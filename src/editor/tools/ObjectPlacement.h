#pragma once

#include "core/model/HmiObject.h"
#include "core/model/Ids.h"

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <memory>
#include <optional>

namespace hmi
{

class ToolContext;

// Creating new objects for the create tool and toolbox drag and drop.
// Geometry is rounded to whole scene pixels.
namespace ObjectPlacement
{

// Default size of a type, from its descriptors; empty if unknown.
[[nodiscard]] QSizeF defaultSize(ObjectTypeId type);

// New object with its default size centred on `sceneCentre`.
[[nodiscard]] std::unique_ptr<HmiObject> createCentred(ObjectTypeId type, QPointF sceneCentre);

// New object filling `sceneRect` (normalized). `rising` picks the bottom-left
// to top-right diagonal for types that have a line direction.
[[nodiscard]] std::unique_ptr<HmiObject> createInRect(ObjectTypeId type, const QRectF& sceneRect,
                                                      bool rising);

// Adds the object to the current screen with one command and selects it.
std::optional<ObjectId> add(ToolContext& context, std::unique_ptr<HmiObject> object);

} // namespace ObjectPlacement

} // namespace hmi
