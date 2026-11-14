#include "core/objects/TextBox.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <array>

namespace hmi
{

namespace
{

constexpr std::array kHorizontalOptions{
    EnumOption{.value = TextBox::AlignLeft,
               .key = "left",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Left")},
    EnumOption{.value = TextBox::AlignHCenter,
               .key = "center",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Centre")},
    EnumOption{.value = TextBox::AlignRight,
               .key = "right",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Right")},
};

constexpr std::array kVerticalOptions{
    EnumOption{.value = TextBox::AlignTop,
               .key = "top",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Top")},
    EnumOption{.value = TextBox::AlignVCenter,
               .key = "middle",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Middle")},
    EnumOption{.value = TextBox::AlignBottom,
               .key = "bottom",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Bottom")},
};

constexpr int kDefaultPixelSize = 16;

QFont defaultFont()
{
    QFont font;
    font.setPixelSize(kDefaultPixelSize);
    return font;
}

} // namespace

TextBox::TextBox(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> TextBox::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 32.0},
        {
            {.id = Prop::Text,
             .key = "text",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Text"),
             .category = PropertyCategory::Text,
             .defaultValue = QStringLiteral("Text"),
             .bindable = true},
            {.id = Prop::Font,
             .key = "font",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Font"),
             .category = PropertyCategory::Text,
             .defaultValue = defaultFont()},
            {.id = Prop::TextColor,
             .key = "textColor",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Text colour"),
             .category = PropertyCategory::Text,
             .defaultValue = QColor(0x1F, 0x23, 0x28),
             .bindable = true},
            {.id = Prop::HorizontalAlignment,
             .key = "horizontalAlignment",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Horizontal alignment"),
             .category = PropertyCategory::Text,
             .defaultValue = EnumValue{AlignLeft},
             .constraint = {.enumOptions = kHorizontalOptions}},
            {.id = Prop::VerticalAlignment,
             .key = "verticalAlignment",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Vertical alignment"),
             .category = PropertyCategory::Text,
             .defaultValue = EnumValue{AlignVCenter},
             .constraint = {.enumOptions = kVerticalOptions}},
            {.id = Prop::WordWrap,
             .key = "wordWrap",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Word wrap"),
             .category = PropertyCategory::Text,
             .defaultValue = true},
            {.id = Prop::FillColor,
             .key = "fillColor",
             .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Background"),
             .category = Appearance,
             .defaultValue = QColor(Qt::transparent),
             .bindable = true},
        });
    return table;
}

std::unique_ptr<HmiObject> TextBox::clone() const
{
    return std::unique_ptr<HmiObject>(new TextBox(*this));
}

int TextBox::textFlags() const
{
    int flags = 0;
    switch (value<EnumValue>(Prop::HorizontalAlignment).value)
    {
    case AlignHCenter:
        flags |= Qt::AlignHCenter;
        break;
    case AlignRight:
        flags |= Qt::AlignRight;
        break;
    default:
        flags |= Qt::AlignLeft;
        break;
    }
    switch (value<EnumValue>(Prop::VerticalAlignment).value)
    {
    case AlignTop:
        flags |= Qt::AlignTop;
        break;
    case AlignBottom:
        flags |= Qt::AlignBottom;
        break;
    default:
        flags |= Qt::AlignVCenter;
        break;
    }
    if (value<bool>(Prop::WordWrap))
        flags |= Qt::TextWordWrap;
    return flags;
}

void TextBox::paint(QPainter& painter, const PaintContext& context) const
{
    const QRectF bounds(QPointF(0, 0), geometry().size());
    const QColor& background = value<QColor>(Prop::FillColor);
    if (background.alpha() > 0)
        painter.fillRect(bounds, background);

    // At low zoom the glyphs would be unreadable smudges; skip them.
    if (context.lowDetail())
        return;

    painter.setPen(value<QColor>(Prop::TextColor));
    painter.setFont(value<QFont>(Prop::Font));
    painter.drawText(bounds, textFlags(), value<QString>(Prop::Text));
}

void TextBox::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::TextBox, "textBox", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Text box"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/text-box.svg")
