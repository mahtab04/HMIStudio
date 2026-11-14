#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include <vector>

namespace hmi
{

struct ObjectTypeInfo;

// Toolbox entries from the ObjectRegistry, grouped by category. Read-only;
// placing an object goes through EditorController or ToolboxDragController.
class ToolboxModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ToolboxModel is provided by the application")

public:
    enum Role
    {
        KeyRole = Qt::UserRole + 1,
        NameRole,
        IconSourceRole,
        CategoryRole,
    };
    Q_ENUM(Role)

    explicit ToolboxModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<const ObjectTypeInfo*> m_types;
};

} // namespace hmi
