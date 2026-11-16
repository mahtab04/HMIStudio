#include "render/GraphicCache.h"

#include <QHash>

namespace hmi
{

size_t qHash(const GraphicCache::Key& key, size_t seed) noexcept
{
    return qHashMulti(seed, key.assetId, key.pixelSize.width(), key.pixelSize.height(),
                      key.styleHash);
}

GraphicCache::GraphicCache(qsizetype budgetBytes)
    : m_images(budgetBytes / 1024)
{
}

QImage GraphicCache::find(const Key& key) const
{
    const std::scoped_lock lock(m_mutex);
    const QImage* image = m_images.object(key);
    return image ? *image : QImage();
}

void GraphicCache::insert(const Key& key, const QImage& image)
{
    const std::scoped_lock lock(m_mutex);
    m_images.insert(key, new QImage(image), std::max<qsizetype>(1, image.sizeInBytes() / 1024));
}

void GraphicCache::clear()
{
    const std::scoped_lock lock(m_mutex);
    m_images.clear();
}

int GraphicCache::count() const
{
    const std::scoped_lock lock(m_mutex);
    return int(m_images.count());
}

} // namespace hmi
