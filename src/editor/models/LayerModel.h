#pragma once

#include "core/model/Ids.h"

#include <QAbstractListModel>
#include <QSet>
#include <qqmlintegration.h>

#include <vector>

namespace hmi
{

class HmiObject;
class ToolContext;

// The objects of the current screen as layers, topmost first, for the
// Layers panel. Groups can be expanded to show their children, indented by
// `depth`; children are shown for orientation and cannot be edited on their
// own. A flat list rather than a tree model, so a plain ListView can show it
// and reorder by dragging.
//
// Rename, show/hide, lock and reorder push commands; selecting a row selects
// the object on the canvas.
class LayerModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LayerModel is provided by the application")

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        IconRole,
        DepthRole,
        HasChildrenRole,
        ExpandedRole,
        ObjectVisibleRole,
        LockedRole,
        SelectedRole,
        EditableRole,
    };

    explicit LayerModel(ToolContext& context, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Selects the row's object (a child row selects its top-level group).
    // `additive` toggles it in the selection instead.
    Q_INVOKABLE void select(int row, bool additive);
    Q_INVOKABLE void toggleExpanded(int row);
    Q_INVOKABLE void setObjectVisible(int row, bool visible);
    Q_INVOKABLE void setLocked(int row, bool locked);
    Q_INVOKABLE void rename(int row, const QString& name);
    // Moves a top-level row `delta` rows up (negative) or down in the list,
    // that is towards the front or the back. Returns false if nothing moved.
    Q_INVOKABLE bool moveBy(int row, int delta);

    // Rebuilds the rows for the current screen.
    void refresh();

private:
    struct Row
    {
        const HmiObject* object = nullptr;
        int depth = 0;
        int topLevel = 0; // position of the row's top-level object, topmost = 0
    };

    void addRows(const HmiObject& object, int depth, int topLevel);
    void onPropertyChanged(ObjectId object, PropertyId property);
    void setProperty(int row, PropertyId property, const QVariant& value);
    [[nodiscard]] const Row* rowAt(int row) const;

    ToolContext& m_context;
    std::vector<Row> m_rows;
    QSet<ObjectId> m_expanded;
};

} // namespace hmi
