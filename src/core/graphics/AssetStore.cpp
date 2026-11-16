#include "core/graphics/AssetStore.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace hmi
{

QString AssetStore::idFor(const QByteArray& data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().left(32));
}

QString AssetStore::add(QByteArray data, const QString& fileName)
{
    const QString id = idFor(data);
    const std::scoped_lock lock(m_mutex);
    if (!m_assets.contains(id))
    {
        auto asset = std::make_shared<Asset>();
        asset->id = id;
        asset->fileName = fileName;
        asset->suffix = QFileInfo(fileName).suffix().toLower();
        asset->data = std::move(data);
        m_assets.emplace(id, std::move(asset));
    }
    return id;
}

Result<QString> AssetStore::addFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return makeError(ErrorCode::Io,
                         QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()));
    }
    return add(file.readAll(), QFileInfo(path).fileName());
}

std::shared_ptr<const Asset> AssetStore::find(const QString& id) const
{
    const std::scoped_lock lock(m_mutex);
    const auto it = m_assets.find(id);
    return it == m_assets.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<const Asset>> AssetStore::all() const
{
    const std::scoped_lock lock(m_mutex);
    std::vector<std::shared_ptr<const Asset>> assets;
    assets.reserve(m_assets.size());
    for (const auto& entry : m_assets)
        assets.push_back(entry.second);
    return assets;
}

std::size_t AssetStore::size() const
{
    const std::scoped_lock lock(m_mutex);
    return m_assets.size();
}

void AssetStore::clear()
{
    const std::scoped_lock lock(m_mutex);
    m_assets.clear();
}

void AssetStore::takeFrom(AssetStore& other)
{
    if (&other == this)
        return;
    const std::scoped_lock lock(m_mutex, other.m_mutex);
    m_assets = std::move(other.m_assets);
    other.m_assets.clear();
}

} // namespace hmi
