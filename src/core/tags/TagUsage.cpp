#include "core/tags/TagUsage.h"

#include "core/model/Project.h"

namespace hmi::TagUsage
{

namespace
{

void collect(const HmiObject& object, ScreenId screen, ObjectId topLevel, const QString& tag,
             std::vector<TagUse>& uses)
{
    for (const Binding& binding : object.bindings())
    {
        if (binding.tag.compare(tag, Qt::CaseInsensitive) == 0)
            uses.push_back({screen, topLevel, binding.property});
    }
    for (const auto& descriptor : object.descriptors())
    {
        if (valueKind(descriptor.defaultValue) == ValueKind::Tag &&
            object.value<TagRef>(descriptor.id).tagName.compare(tag, Qt::CaseInsensitive) == 0)
            uses.push_back({screen, topLevel, descriptor.id});
    }
    for (const auto& child : object.children())
        collect(*child, screen, topLevel, tag, uses);
}

} // namespace

std::vector<TagUse> find(const Project& project, const QString& tag)
{
    std::vector<TagUse> uses;
    for (const auto& screen : project.screens())
    {
        for (const auto& object : screen->objects())
            collect(*object, screen->id(), object->id(), tag, uses);
    }
    return uses;
}

} // namespace hmi::TagUsage
