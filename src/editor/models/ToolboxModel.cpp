#include "editor/models/ToolboxModel.h"

#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QUrl>
#include <QVariantMap>

namespace hmi
{

namespace
{

QString categoryName(ObjectCategory category)
{
    switch (category)
    {
    case ObjectCategory::BasicObjects:
        return QCoreApplication::translate("hmi::ToolboxModel", "Basic objects");
    case ObjectCategory::Elements:
        return QCoreApplication::translate("hmi::ToolboxModel", "Elements");
    case ObjectCategory::Controls:
        return QCoreApplication::translate("hmi::ToolboxModel", "Controls");
    case ObjectCategory::ProcessSymbols:
        return QCoreApplication::translate("hmi::ToolboxModel", "Process symbols");
    }
    return {};
}

QString displayName(const ObjectTypeInfo& info)
{
    return QCoreApplication::translate("hmi::ObjectRegistry", info.displayName);
}

} // namespace

ToolboxModel::ToolboxModel(QObject* parent)
    : QAbstractListModel(parent)
{
    rebuild();
}

void ToolboxModel::rebuild()
{
    m_sections.clear();
    for (const ObjectCategory category : {ObjectCategory::BasicObjects, ObjectCategory::Elements,
                                          ObjectCategory::Controls, ObjectCategory::ProcessSymbols})
    {
        Section section{.title = categoryName(category), .types = {}};
        for (const ObjectTypeInfo* info : ObjectRegistry::instance().types(category))
        {
            if (!info->inToolbox)
                continue;
            const QString filter = m_filter.trimmed();
            if (filter.isEmpty() || displayName(*info).contains(filter, Qt::CaseInsensitive) ||
                info->key.contains(filter, Qt::CaseInsensitive))
                section.types.push_back(info);
        }
        if (!section.types.empty())
            m_sections.push_back(std::move(section));
    }
}

void ToolboxModel::setFilterText(const QString& text)
{
    if (text == m_filter)
        return;
    beginResetModel();
    m_filter = text;
    rebuild();
    endResetModel();
    Q_EMIT filterTextChanged();
}

int ToolboxModel::entryCount() const
{
    int count = 0;
    for (const Section& section : m_sections)
        count += int(section.types.size());
    return count;
}

int ToolboxModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_sections.size());
}

QVariant ToolboxModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Section& section = m_sections[std::size_t(index.row())];
    switch (role)
    {
    case Qt::DisplayRole:
    case TitleRole:
        return section.title;
    case TilesRole:
    {
        QVariantList tiles;
        for (const ObjectTypeInfo* info : section.types)
        {
            tiles.append(QVariantMap{
                {QStringLiteral("key"), info->key},
                {QStringLiteral("name"), displayName(*info)},
                {QStringLiteral("iconSource"), QUrl(QStringLiteral("qrc") + info->iconPath)},
            });
        }
        return tiles;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> ToolboxModel::roleNames() const
{
    return {{TitleRole, "title"}, {TilesRole, "tiles"}};
}

} // namespace hmi
