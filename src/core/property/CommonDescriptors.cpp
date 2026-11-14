#include "core/property/CommonDescriptors.h"

#include "core/property/Prop.h"

#include <QtGlobal>

namespace hmi
{

std::vector<PropertyDescriptor>
withCommonDescriptors(QSizeF defaultSize, std::initializer_list<PropertyDescriptor> specific)
{
    using namespace PropertyCategory;

    std::vector<PropertyDescriptor> table{
        {.id = Prop::Name,
         .key = "name",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Name"),
         .category = General,
         .defaultValue = QString()},
        {.id = Prop::X,
         .key = "x",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "X"),
         .category = Layout,
         .defaultValue = 0.0,
         .bindable = true},
        {.id = Prop::Y,
         .key = "y",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Y"),
         .category = Layout,
         .defaultValue = 0.0,
         .bindable = true},
        {.id = Prop::Width,
         .key = "width",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Width"),
         .category = Layout,
         .defaultValue = defaultSize.width(),
         .constraint = {.minimum = 0.0},
         .bindable = true},
        {.id = Prop::Height,
         .key = "height",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Height"),
         .category = Layout,
         .defaultValue = defaultSize.height(),
         .constraint = {.minimum = 0.0},
         .bindable = true},
        {.id = Prop::Rotation,
         .key = "rotation",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Rotation"),
         .category = Layout,
         .defaultValue = 0.0,
         .constraint = {.minimum = -360.0, .maximum = 360.0},
         .bindable = true},
        {.id = Prop::Visible,
         .key = "visible",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Visible"),
         .category = Behaviour,
         .defaultValue = true,
         .bindable = true},
        {.id = Prop::Locked,
         .key = "locked",
         .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Locked"),
         .category = Behaviour,
         .defaultValue = false},
    };
    table.insert(table.end(), specific.begin(), specific.end());

#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        Q_ASSERT_X(PropertyId::fromKey(QLatin1StringView(table[i].key)) == table[i].id,
                   "withCommonDescriptors", "descriptor key does not match its id");
        for (std::size_t j = i + 1; j < table.size(); ++j)
            Q_ASSERT_X(table[i].id != table[j].id, "withCommonDescriptors", "duplicate id");
    }
#endif
    return table;
}

} // namespace hmi
