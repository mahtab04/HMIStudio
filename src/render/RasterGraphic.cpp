#include "render/RasterGraphic.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>

namespace hmi
{

RasterGraphic::RasterGraphic(const QByteArray& data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    m_image = reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QSizeF RasterGraphic::naturalSize() const
{
    return m_image.size();
}

void RasterGraphic::render(QPainter& painter, const QRectF& target,
                           const GraphicStyle& /*style*/) const
{
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, m_image);
    painter.restore();
}

} // namespace hmi
