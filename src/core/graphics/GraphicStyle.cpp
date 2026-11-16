#include "core/graphics/GraphicStyle.h"

#include <QHash>
#include <QStringList>

#include <algorithm>

namespace hmi
{

const GraphicStyle::Override* GraphicStyle::find(QStringView elementId) const
{
    const auto it = std::ranges::find(overrides, elementId, &Override::elementId);
    return it == overrides.end() ? nullptr : &*it;
}

std::size_t GraphicStyle::hash() const
{
    return overrides.empty() ? 0 : qHash(toString());
}

Result<GraphicStyle> GraphicStyle::parse(const QString& text)
{
    GraphicStyle style;
    const QStringList entries = text.split(u';', Qt::SkipEmptyParts);
    for (const QString& raw : entries)
    {
        const QString entry = raw.trimmed();
        if (entry.isEmpty())
            continue;
        const qsizetype colon = entry.indexOf(u':');
        const QString id = entry.left(colon).trimmed();
        const QStringList words = entry.mid(colon + 1).split(u' ', Qt::SkipEmptyParts);
        if (colon <= 0 || id.isEmpty() || words.isEmpty())
        {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Style entry '%1' should read 'id: fill #RRGGBB', "
                                            "'id: stroke #RRGGBB' or 'id: hidden'")
                                 .arg(entry));
        }

        Override* target = nullptr;
        if (auto it = std::ranges::find(style.overrides, id, &Override::elementId);
            it != style.overrides.end())
            target = &*it;
        else
            target = &style.overrides.emplace_back(Override{.elementId = id});

        const QString what = words.front().toLower();
        if (what == u"hidden" && words.size() == 1)
        {
            target->hidden = true;
            continue;
        }
        const QColor color = words.size() == 2 ? QColor::fromString(words[1]) : QColor();
        if ((what != u"fill" && what != u"stroke") || !color.isValid())
        {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Style entry '%1' should read 'id: fill #RRGGBB', "
                                            "'id: stroke #RRGGBB' or 'id: hidden'")
                                 .arg(entry));
        }
        (what == u"fill" ? target->fill : target->stroke) = color;
    }
    return style;
}

QString GraphicStyle::toString() const
{
    QStringList parts;
    for (const Override& o : overrides)
    {
        if (o.fill)
            parts << QStringLiteral("%1: fill %2").arg(o.elementId, o.fill->name(QColor::HexArgb));
        if (o.stroke)
            parts << QStringLiteral("%1: stroke %2")
                         .arg(o.elementId, o.stroke->name(QColor::HexArgb));
        if (o.hidden)
            parts << QStringLiteral("%1: hidden").arg(o.elementId);
    }
    return parts.join(QStringLiteral("; "));
}

} // namespace hmi
