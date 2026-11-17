#pragma once

#include "core/model/Events.h"
#include "core/model/Ids.h"

#include <QString>

#include <optional>
#include <vector>

namespace hmi
{

class Project;

// One place a tag is used: a bound property of an object on a screen, or an
// action of one of its events (inside a group, `object` is the top-level
// group).
struct TagUse
{
    ScreenId screen;
    ObjectId object;
    PropertyId property;              // a binding or tag property
    std::optional<EventKind> event{}; // or an event's action
    friend bool operator==(const TagUse&, const TagUse&) = default;
};

// "Where used" for tags. Bindings live on HmiObject itself, so a walk over
// screens, objects and group children finds them without a visitor per type.
namespace TagUsage
{

// Every use of `tag` (case-insensitive), screen by screen in z-order.
[[nodiscard]] std::vector<TagUse> find(const Project& project, const QString& tag);

} // namespace TagUsage

} // namespace hmi
