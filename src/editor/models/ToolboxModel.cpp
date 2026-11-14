#include "editor/models/ToolboxModel.h"

#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QUrl>

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

} // namespace

ToolboxModel::ToolboxModel(QObject* parent)
    : QAbstractListModel(parent)
{
    for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        m_types.push_back(&info);
}

int ToolboxModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_types.size());
}

QVariant ToolboxModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const ObjectTypeInfo& info = *m_types[std::size_t(index.row())];
    switch (role)
    {
    case KeyRole:
        return info.key;
    case Qt::DisplayRole:
    case NameRole:
        return QCoreApplication::translate("hmi::ObjectRegistry", info.displayName);
    case IconSourceRole:
        return QUrl(QStringLiteral("qrc") + info.iconPath);
    case CategoryRole:
        return categoryName(info.category);
    default:
        return {};
    }
}

QHash<int, QByteArray> ToolboxModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {NameRole, "name"},
        {IconSourceRole, "iconSource"},
        {CategoryRole, "category"},
    };
}

} // namespace hmi
