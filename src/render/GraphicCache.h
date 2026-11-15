#pragma once

#include <QCache>
#include <QImage>
#include <QSize>
#include <QString>

#include <mutex>

namespace hmi
{

// Rendered graphics as QImage (not QPixmap: painting runs on the render
// thread), keyed by asset, pixel size and style (ARCHITECTURE.md section
// 14.2). The pixel size already includes zoom and device pixel ratio.
// Least recently used images go first once `budgetBytes` is reached.
// Thread-safe.
class GraphicCache
{
public:
    struct Key
    {
        QString assetId;
        QSize pixelSize;
        std::size_t styleHash = 0;
        friend bool operator==(const Key&, const Key&) = default;
    };

    static constexpr qsizetype kDefaultBudgetBytes = 64 * 1024 * 1024;

    explicit GraphicCache(qsizetype budgetBytes = kDefaultBudgetBytes);

    [[nodiscard]] QImage find(const Key& key) const;
    void insert(const Key& key, const QImage& image);
    void clear();
    [[nodiscard]] int count() const;

private:
    mutable std::mutex m_mutex;
    // Costs are in kilobytes so large budgets fit QCache's int range.
    mutable QCache<Key, QImage> m_images;
};

size_t qHash(const GraphicCache::Key& key, size_t seed = 0) noexcept;

} // namespace hmi
