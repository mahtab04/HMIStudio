#include "render/SvgInspector.h"

#include "render/ISvgBackend.h"

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QSet>
#include <QXmlStreamReader>

#include <cmath>

namespace hmi::SvgInspector
{

namespace
{

// Elements Qt SVG (Tiny 1.2 plus recent additions) ignores or draws wrongly.
const QSet<QString>& unsupported()
{
    static const QSet<QString> names{
        QStringLiteral("filter"),        QStringLiteral("feGaussianBlur"),
        QStringLiteral("feOffset"),      QStringLiteral("feBlend"),
        QStringLiteral("feColorMatrix"), QStringLiteral("feComposite"),
        QStringLiteral("feDropShadow"),  QStringLiteral("feMerge"),
        QStringLiteral("feFlood"),       QStringLiteral("feTurbulence"),
        QStringLiteral("foreignObject"), QStringLiteral("script"),
        QStringLiteral("animate"),       QStringLiteral("animateTransform"),
        QStringLiteral("animateMotion"), QStringLiteral("set"),
        QStringLiteral("iframe"),        QStringLiteral("video"),
        QStringLiteral("audio"),
    };
    return names;
}

} // namespace

QStringList unsupportedElements(const QByteArray& svg)
{
    QSet<QString> found;
    QXmlStreamReader reader(svg);
    while (!reader.atEnd())
    {
        if (reader.readNext() == QXmlStreamReader::StartElement)
        {
            const QString name = reader.name().toString();
            if (unsupported().contains(name))
                found.insert(name);
        }
    }
    QStringList list(found.begin(), found.end());
    list.sort();
    return list;
}

QByteArray rasterize(const QByteArray& svg, QSize pixelSize)
{
    auto backend = createSvgBackend();
    if (!backend->load(svg))
        return {};
    if (pixelSize.isEmpty())
    {
        const QSizeF natural = backend->naturalSize();
        pixelSize = QSize(std::max(1, int(std::ceil(natural.width()))),
                          std::max(1, int(std::ceil(natural.height()))));
    }
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    backend->render(painter, QRectF(QPointF(0, 0), QSizeF(pixelSize)));
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

} // namespace hmi::SvgInspector
