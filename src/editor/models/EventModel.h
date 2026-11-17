#pragma once

#include "core/model/Events.h"
#include "core/model/Ids.h"

#include <QAbstractListModel>
#include <QStringList>
#include <qqmlintegration.h>

namespace hmi
{

class HmiObject;
class ToolContext;

// The Events tab of the properties panel: the actions of one event (Click,
// Press, Release) of the selected object, one row per action, in the order
// they run. Every edit pushes one SetActionsCommand.
class EventModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("EventModel is provided by the application")
    // Exactly one object is selected.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    // Index into eventNames.
    Q_PROPERTY(int event READ event WRITE setEvent NOTIFY eventChanged)
    // "Click", or "Click (2)" when it has actions.
    Q_PROPERTY(QStringList eventNames READ eventNames NOTIFY namesChanged)
    Q_PROPERTY(QStringList actionNames READ actionNames CONSTANT)
    Q_PROPERTY(QStringList tagNames READ tagNames NOTIFY namesChanged)
    // The project's screens other than the one being edited.
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY namesChanged)

public:
    enum Role
    {
        KindRole = Qt::UserRole + 1, // index into actionNames
        TagRole,
        ValueRole,
        ScreenRole,
        // "tag", "tagValue" (set/increment), "screen", "none": which fields
        // to show.
        FieldsRole,
    };

    explicit EventModel(ToolContext& context, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool available() const { return object() != nullptr; }
    [[nodiscard]] int event() const { return int(m_event); }
    void setEvent(int event);
    [[nodiscard]] QStringList eventNames() const;
    [[nodiscard]] QStringList actionNames() const { return Events::actionNames(); }
    [[nodiscard]] QStringList tagNames() const;
    [[nodiscard]] QStringList screenNames() const;

    // Adds an action of `kind` at the end.
    Q_INVOKABLE void addAction(int kind);
    Q_INVOKABLE void removeAction(int row);
    // Moves an action up (-1) or down (+1) in the list.
    Q_INVOKABLE void moveAction(int row, int delta);
    Q_INVOKABLE void setKind(int row, int kind);
    Q_INVOKABLE void setTag(int row, const QString& tag);
    Q_INVOKABLE void setValue(int row, const QString& value);
    Q_INVOKABLE void setScreen(int row, const QString& screen);

    // Rebuilds for the current selection and screen.
    void refresh();

Q_SIGNALS:
    void availableChanged();
    void eventChanged();
    void namesChanged();

private:
    [[nodiscard]] HmiObject* object() const;
    [[nodiscard]] const ActionList& actions() const;
    void change(ActionList after);
    template <typename Edit>
    void editRow(int row, Edit edit);

    ToolContext& m_context;
    EventKind m_event = EventKind::Click;
    ActionList m_rows; // shown, a copy of the object's list
    bool m_available = false;
};

} // namespace hmi
