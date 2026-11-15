#include "render/GraphicLibrary.h"

#include "core/graphics/AssetStore.h"
#include "render/GraphicFactory.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

// Images larger than this are drawn directly instead of cached.
constexpr int kMaxCachedSide = 4096;

} // namespace

GraphicLibrary::GraphicLibrary(const AssetStore& assets)
    : m_assets(assets)
{
}

GraphicLibrary::~GraphicLibrary() = default;

std::shared_ptr<const IGraphicSource> GraphicLibrary::source(const QString& assetId) const
{
    {
        const std::scoped_lock lock(m_mutex);
        if (const auto it = m_sources.find(assetId); it != m_sources.end())
            return it->second;
    }
    const auto asset = m_assets.find(assetId);
    if (!asset)
        return nullptr;
    // Decode outside the lock; another thread may race, and one result wins.
    std::shared_ptr<const IGraphicSource> created = GraphicFactory::create(*asset);
    const std::scoped_lock lock(m_mutex);
    return m_sources.emplace(assetId, std::move(created)).first->second;
}

QSizeF GraphicLibrary::naturalSize(const QString& assetId) const
{
    const auto s = source(assetId);
    return s ? s->naturalSize() : QSizeF();
}

QRectF GraphicLibrary::fitted(const QRectF& target, QSizeF natural, GraphicFit fit)
{
    if (natural.isEmpty() || fit == GraphicFit::Stretch)
        return target;
    QSizeF size = natural;
    switch (fit)
    {
    case GraphicFit::Fit:
        size = natural.scaled(target.size(), Qt::KeepAspectRatio);
        break;
    case GraphicFit::Fill:
        size = natural.scaled(target.size(), Qt::KeepAspectRatioByExpanding);
        break;
    case GraphicFit::Centre:
    case GraphicFit::Stretch:
        break;
    }
    return {target.center() - QPointF(size.width() / 2.0, size.height() / 2.0), size};
}

bool GraphicLibrary::draw(QPainter& painter, const QString& assetId, const QRectF& target,
                          GraphicFit fit, const GraphicStyle& style) const
{
    const auto graphic = source(assetId);
    if (!graphic)
        return false;
    const QRectF rect = fitted(target, graphic->naturalSize(), fit);

    // Render at the size the pixels will have on screen, so the result is
    // sharp at every zoom and is not rendered again until that changes.
    const QRectF device = painter.deviceTransform().mapRect(rect);
    const QSize pixels(std::max(1, int(std::ceil(device.width()))),
                       std::max(1, int(std::ceil(device.height()))));

    painter.save();
    painter.setClipRect(target, Qt::IntersectClip);
    if (pixels.width() > kMaxCachedSide || pixels.height() > kMaxCachedSide)
    {
        graphic->render(painter, rect, style);
        painter.restore();
        return true;
    }
    const GraphicCache::Key key{
        .assetId = assetId,
        .pixelSize = pixels,
        .styleHash = graphic->usesStyle() ? style.hash() : 0,
    };
    QImage image = m_cache.find(key);
    if (image.isNull())
    {
        image = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter imagePainter(&image);
        imagePainter.setRenderHint(QPainter::Antialiasing);
        imagePainter.setRenderHint(QPainter::SmoothPixmapTransform);
        graphic->render(imagePainter, QRectF(QPointF(0, 0), QSizeF(pixels)), style);
        imagePainter.end();
        m_cache.insert(key, image);
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(rect, image);
    painter.restore();
    return true;
}

} // namespace hmi
