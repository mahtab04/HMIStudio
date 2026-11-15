#include "core/model/HmiStyle.h"

#include "core/model/HmiObject.h"

namespace hmi
{

namespace
{

const auto kFace = QStringLiteral("face");
const auto kLine = QStringLiteral("line");
const auto kText = QStringLiteral("text");
const auto kAccent = QStringLiteral("accent");
const auto kFontFamily = QStringLiteral("fontFamily");

QColor colourOr(const QJsonObject& json, const QString& key, const QColor& fallback)
{
    const QColor colour = QColor::fromString(json.value(key).toString());
    return colour.isValid() ? colour : fallback;
}

} // namespace

bool HmiStyle::applyTo(HmiObject& object) const
{
    bool changed = false;
    for (const PropertyDescriptor& d : object.descriptors())
    {
        std::optional<PropertyValue> value;
        switch (d.styleRole)
        {
        case StyleRole::Face:
            value = face;
            break;
        case StyleRole::Line:
            value = line;
            break;
        case StyleRole::Text:
            value = text;
            break;
        case StyleRole::Accent:
            value = accent;
            break;
        case StyleRole::Font:
            if (!fontFamily.isEmpty())
            {
                QFont font = object.value<QFont>(d.id);
                font.setFamily(fontFamily);
                value = font;
            }
            break;
        case StyleRole::None:
            break;
        }
        if (value && object.property(d.id) != value && object.setProperty(d.id, *value))
            changed = true;
    }
    return changed;
}

QJsonObject HmiStyle::toJson() const
{
    return {
        {kFace, face.name(QColor::HexArgb)}, {kLine, line.name(QColor::HexArgb)},
        {kText, text.name(QColor::HexArgb)}, {kAccent, accent.name(QColor::HexArgb)},
        {kFontFamily, fontFamily},
    };
}

HmiStyle HmiStyle::fromJson(const QJsonObject& json)
{
    HmiStyle style;
    style.face = colourOr(json, kFace, style.face);
    style.line = colourOr(json, kLine, style.line);
    style.text = colourOr(json, kText, style.text);
    style.accent = colourOr(json, kAccent, style.accent);
    style.fontFamily = json.value(kFontFamily).toString();
    return style;
}

} // namespace hmi
