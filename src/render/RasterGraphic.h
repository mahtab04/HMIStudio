#pragma once

#include "render/IGraphicSource.h"

#include <QImage>

namespace hmi
{

// PNG, JPEG, BMP, GIF, WebP and the other formats QImageReader supports,
// decoded once.
class RasterGraphic final : public IGraphicSource
{
public:
    // Decodes `data`; isNull() tells whether that worked.
    explicit RasterGraphic(const QByteArray& data);

    [[nodiscard]] bool isNull() const { return m_image.isNull(); }
    [[nodiscard]] QSizeF naturalSize() const override;
    void render(QPainter& painter, const QRectF& target, const GraphicStyle& style) const override;
    [[nodiscard]] bool usesStyle() const override { return false; }

private:
    QImage m_image;
};

} // namespace hmi
