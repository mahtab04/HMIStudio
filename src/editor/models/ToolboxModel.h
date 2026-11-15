#pragma once

#include <QAbstractListModel>
#include <qqmlintegration.h>

#include <vector>

namespace hmi
{

struct ObjectTypeInfo;

// The toolbox: one row per section (basic objects, elements, controls,
// process symbols) with the section's entries as `tiles`, filtered by
// `filterText`. Sections without a matching entry are left out. Read-only;
// placing an object goes through EditorController or ToolboxDragController.
class ToolboxModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ToolboxModel is provided by the application")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY filterTextChanged)

public:
    enum Role
    {
        TitleRole = Qt::UserRole + 1,
        // List of {key, name, iconSource} maps.
        TilesRole,
    };
    Q_ENUM(Role)

    explicit ToolboxModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString filterText() const { return m_filter; }
    // Shows only entries whose name or key contains the text, ignoring case.
    void setFilterText(const QString& text);

    // Entries shown, over all sections.
    [[nodiscard]] int entryCount() const;

Q_SIGNALS:
    void filterTextChanged();

private:
    struct Section
    {
        QString title;
        std::vector<const ObjectTypeInfo*> types;
    };

    void rebuild();

    QString m_filter;
    std::vector<Section> m_sections;
};

} // namespace hmi
