#include "render/SvgGraphic.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace hmi
{

namespace
{

// Keeps a variant cache from growing without bound when a binding cycles
// through many colours.
constexpr std::size_t kMaxStyledVariants = 32;

// "fill:#000; stroke:red" without the given keys.
QString withoutKeys(const QString& style, const QStringList& keys)
{
    QStringList kept;
    for (const QString& part : style.split(u';', Qt::SkipEmptyParts))
    {
        const QString key = part.section(u':', 0, 0).trimmed();
        if (!keys.contains(key))
            kept << part.trimmed();
    }
    return kept.join(u';');
}

} // namespace

SvgGraphic::SvgGraphic(QByteArray svg)
    : m_svg(std::move(svg))
{
    auto backend = createSvgBackend();
    if (backend->load(m_svg))
        m_plain = std::move(backend);
}

QSizeF SvgGraphic::naturalSize() const
{
    return m_plain ? m_plain->naturalSize() : QSizeF();
}

QByteArray SvgGraphic::applyStyle(const QByteArray& svg, const GraphicStyle& style)
{
    if (style.isEmpty())
        return svg;
    QByteArray out;
    QXmlStreamReader reader(svg);
    QXmlStreamWriter writer(&out);
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement())
        {
            const GraphicStyle::Override* o = style.find(reader.attributes().value(u"id"));
            if (!o)
            {
                writer.writeCurrentToken(reader);
                continue;
            }
            writer.writeStartElement(reader.namespaceUri().toString(), reader.name().toString());
            QStringList replaced;
            if (o->fill)
                replaced << QStringLiteral("fill") << QStringLiteral("fill-opacity");
            if (o->stroke)
                replaced << QStringLiteral("stroke") << QStringLiteral("stroke-opacity");
            if (o->hidden)
                replaced << QStringLiteral("display") << QStringLiteral("visibility");
            for (const QXmlStreamAttribute& a : reader.attributes())
            {
                const QString name = a.qualifiedName().toString();
                if (replaced.contains(name))
                    continue;
                if (name == u"style")
                {
                    const QString rest = withoutKeys(a.value().toString(), replaced);
                    if (!rest.isEmpty())
                        writer.writeAttribute(name, rest);
                    continue;
                }
                writer.writeAttribute(a);
            }
            if (o->fill)
            {
                writer.writeAttribute(QStringLiteral("fill"), o->fill->name(QColor::HexRgb));
                writer.writeAttribute(QStringLiteral("fill-opacity"),
                                      QString::number(o->fill->alphaF()));
            }
            if (o->stroke)
            {
                writer.writeAttribute(QStringLiteral("stroke"), o->stroke->name(QColor::HexRgb));
                writer.writeAttribute(QStringLiteral("stroke-opacity"),
                                      QString::number(o->stroke->alphaF()));
            }
            if (o->hidden)
                writer.writeAttribute(QStringLiteral("display"), QStringLiteral("none"));
            continue;
        }
        writer.writeCurrentToken(reader);
    }
    return reader.hasError() ? svg : out;
}

const ISvgBackend* SvgGraphic::variant(const GraphicStyle& style) const
{
    if (style.isEmpty() || !m_plain)
        return m_plain.get();
    const std::size_t key = style.hash();
    const std::scoped_lock lock(m_mutex);
    if (const auto it = m_styled.find(key); it != m_styled.end())
        return it->second.get();
    if (m_styled.size() >= kMaxStyledVariants)
        m_styled.clear();
    auto backend = createSvgBackend();
    if (!backend->load(applyStyle(m_svg, style)))
        return m_plain.get();
    return m_styled.emplace(key, std::move(backend)).first->second.get();
}

void SvgGraphic::render(QPainter& painter, const QRectF& target, const GraphicStyle& style) const
{
    if (const ISvgBackend* backend = variant(style))
        backend->render(painter, target);
}

} // namespace hmi
