#pragma once

#include "core/Result.h"

#include <QByteArray>
#include <QString>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace hmi
{

// An imported file (image, SVG) kept by the project. The id is a hash of
// the content, so the same image used on ten screens is stored once and an
// id always means the same bytes.
struct Asset
{
    QString id;       // 32 hex digits of the SHA-256 of `data`
    QString fileName; // original name, for the user
    QString suffix;   // "png", "svg", ... for the saved file
    QByteArray data;
};

// The project's assets (ARCHITECTURE.md section 14.2). Assets are
// immutable and shared: find() hands out shared pointers, so the render
// thread can keep drawing an asset while the GUI thread changes the store.
// All members are thread-safe.
class AssetStore
{
public:
    AssetStore() = default;
    ~AssetStore() = default;
    AssetStore(const AssetStore&) = delete;
    AssetStore& operator=(const AssetStore&) = delete;
    AssetStore(AssetStore&&) = delete;
    AssetStore& operator=(AssetStore&&) = delete;

    [[nodiscard]] static QString idFor(const QByteArray& data);

    // Adds the bytes (if not already there) and returns the asset's id.
    QString add(QByteArray data, const QString& fileName);
    // Reads a file and adds it.
    Result<QString> addFile(const QString& path);

    [[nodiscard]] std::shared_ptr<const Asset> find(const QString& id) const;
    [[nodiscard]] std::vector<std::shared_ptr<const Asset>> all() const;
    [[nodiscard]] std::size_t size() const;

    void clear();
    // Takes over `other`'s assets (used when a project is replaced).
    void takeFrom(AssetStore& other);

private:
    mutable std::mutex m_mutex;
    std::map<QString, std::shared_ptr<const Asset>> m_assets;
};

} // namespace hmi
