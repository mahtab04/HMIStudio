#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

namespace hmi
{

class Project;
class ToolContext;

// The project's screens in order, for the screen tabs. Actions on screens
// (switch, add, rename, ...) go through EditorController.
class ScreenListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ScreenListModel is provided by the application")

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        CurrentRole,
    };

    explicit ScreenListModel(ToolContext& context, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString nameAt(int row) const;

    // The current screen changed (called by the application).
    void currentChanged();

private:
    ToolContext& m_context;
};

} // namespace hmi
