#include "editor/models/ScreenListModel.h"

#include "core/model/Project.h"
#include "editor/tools/ToolContext.h"

namespace hmi
{

ScreenListModel::ScreenListModel(ToolContext& context, QObject* parent)
    : QAbstractListModel(parent)
    , m_context(context)
{
    DocumentNotifier& notifier = m_context.project().notifier();
    // The notifier reports after the change, when rows can no longer be
    // announced in advance; a reset is correct and screens are few.
    const auto reset = [this]
    {
        beginResetModel();
        endResetModel();
    };
    connect(&notifier, &DocumentNotifier::screenAdded, this, reset);
    connect(&notifier, &DocumentNotifier::screenRemoved, this, reset);
    connect(&notifier, &DocumentNotifier::projectReset, this, reset);
    connect(&notifier, &DocumentNotifier::screenChanged, this,
            [this](ScreenId id)
            {
                if (const auto row = m_context.project().indexOf(id))
                    Q_EMIT dataChanged(index(int(*row)), index(int(*row)), {NameRole});
            });
}

int ScreenListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_context.project().screenCount());
}

QVariant ScreenListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Screen& screen = *m_context.project().screens()[std::size_t(index.row())];
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return screen.name();
    case CurrentRole:
        return screen.id() == m_context.screenId();
    default:
        return {};
    }
}

QHash<int, QByteArray> ScreenListModel::roleNames() const
{
    return {{NameRole, "name"}, {CurrentRole, "current"}};
}

QString ScreenListModel::nameAt(int row) const
{
    return data(index(row), NameRole).toString();
}

void ScreenListModel::currentChanged()
{
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0), index(rowCount() - 1), {CurrentRole});
}

} // namespace hmi
