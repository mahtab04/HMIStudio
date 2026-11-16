#include "core/property/DescriptorBuilders.h"

#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"

#include <QFont>

namespace hmi::Describe
{

using namespace PropertyCategory;

PropertyDescriptor fillColor(const QColor& value, const char* displayName)
{
    return {.id = Prop::FillColor,
            .key = "fillColor",
            .displayName =
                displayName ? displayName : QT_TRANSLATE_NOOP("hmi::Property", "Fill colour"),
            .category = Appearance,
            .defaultValue = value,
            .bindable = true};
}

PropertyDescriptor lineColor(const QColor& value)
{
    return {.id = Prop::LineColor,
            .key = "lineColor",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Line colour"),
            .category = Appearance,
            .defaultValue = value,
            .bindable = true};
}

PropertyDescriptor lineWidth(double value)
{
    return {.id = Prop::LineWidth,
            .key = "lineWidth",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Line width"),
            .category = Appearance,
            .defaultValue = value,
            .constraint = {.minimum = 0.0, .maximum = 50.0}};
}

PropertyDescriptor text(const QString& value)
{
    return {.id = Prop::Text,
            .key = "text",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Text"),
            .category = PropertyCategory::Text,
            .defaultValue = value,
            .bindable = true};
}

PropertyDescriptor textColor(const QColor& value, StyleRole role)
{
    return {.id = Prop::TextColor,
            .key = "textColor",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Text colour"),
            .category = PropertyCategory::Text,
            .defaultValue = value,
            .bindable = true,
            .styleRole = role};
}

PropertyDescriptor font(int pixelSize, bool bold)
{
    QFont f;
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    return {.id = Prop::Font,
            .key = "font",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Font"),
            .category = PropertyCategory::Text,
            .defaultValue = f,
            .styleRole = StyleRole::Font};
}

PropertyDescriptor value(double value)
{
    return process(number(Prop::Value, "value", QT_TRANSLATE_NOOP("hmi::Property", "Value"),
                          Behaviour, value, std::nullopt, std::nullopt, true));
}

PropertyDescriptor minimum(double value)
{
    return number(Prop::Minimum, "minimum", QT_TRANSLATE_NOOP("hmi::Property", "Minimum"),
                  Behaviour, value);
}

PropertyDescriptor maximum(double value)
{
    return number(Prop::Maximum, "maximum", QT_TRANSLATE_NOOP("hmi::Property", "Maximum"),
                  Behaviour, value);
}

PropertyDescriptor number(PropertyId id, const char* key, const char* displayName,
                          const char* category, double value, std::optional<double> minimum,
                          std::optional<double> maximum, bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = value,
            .constraint = {.minimum = minimum, .maximum = maximum},
            .bindable = bindable};
}

PropertyDescriptor integer(PropertyId id, const char* key, const char* displayName,
                           const char* category, int value, std::optional<double> minimum,
                           std::optional<double> maximum, bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = value,
            .constraint = {.minimum = minimum, .maximum = maximum},
            .bindable = bindable};
}

PropertyDescriptor flag(PropertyId id, const char* key, const char* displayName,
                        const char* category, bool value, bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = value,
            .bindable = bindable};
}

PropertyDescriptor string(PropertyId id, const char* key, const char* displayName,
                          const char* category, const QString& value, bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = value,
            .bindable = bindable};
}

PropertyDescriptor color(PropertyId id, const char* key, const char* displayName,
                         const char* category, const QColor& value, bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = value,
            .bindable = bindable};
}

PropertyDescriptor choice(PropertyId id, const char* key, const char* displayName,
                          const char* category, int value, std::span<const EnumOption> options,
                          bool bindable)
{
    return {.id = id,
            .key = key,
            .displayName = displayName,
            .category = category,
            .defaultValue = EnumValue{value},
            .constraint = {.enumOptions = options},
            .bindable = bindable};
}

PropertyDescriptor points(const PointList& value, int minimumCount)
{
    return {.id = Prop::Points,
            .key = "points",
            .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Points"),
            .category = Layout,
            .defaultValue = value,
            .constraint = {.minimum = double(minimumCount)}};
}

namespace
{
constexpr EnumOption kOrientationOptions[] = {
    {.value = kHorizontal,
     .key = "horizontal",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Horizontal")},
    {.value = kVertical,
     .key = "vertical",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Vertical")},
};
} // namespace

PropertyDescriptor process(PropertyDescriptor descriptor)
{
    descriptor.process = true;
    descriptor.bindable = true;
    return descriptor;
}

PropertyDescriptor styled(PropertyDescriptor descriptor, StyleRole role)
{
    descriptor.styleRole = role;
    return descriptor;
}

PropertyDescriptor orientation(int value)
{
    return choice(Prop::Orientation, "orientation",
                  QT_TRANSLATE_NOOP("hmi::Property", "Orientation"), Layout, value,
                  kOrientationOptions);
}

namespace
{
constexpr EnumOption kStretchOptions[] = {
    {.value = 0,
     .key = "keepProportions",
     .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Keep proportions")},
    {.value = 1, .key = "stretch", .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Stretch")},
};
} // namespace

PropertyDescriptor stretchMode(int value)
{
    return choice(Prop::StretchMode, "stretchMode",
                  QT_TRANSLATE_NOOP("hmi::Property", "Stretch mode"), Layout, value,
                  kStretchOptions);
}

PropertyDescriptor items(const QString& value)
{
    return string(Prop::Items, "items", QT_TRANSLATE_NOOP("hmi::Property", "Entries (a;b;c)"),
                  Behaviour, value, true);
}

QStringList splitItems(const QString& items)
{
    QStringList list;
    for (const QString& item : items.split(u';'))
        list << item.trimmed();
    if (list.size() == 1 && list.front().isEmpty())
        list.clear();
    return list;
}

} // namespace hmi::Describe
