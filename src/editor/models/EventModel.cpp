#include "editor/models/EventModel.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "editor/commands/EventCommands.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QUndoStack>

namespace hmi
{

EventModel::EventModel(ToolContext& context, QObject* parent)
    : QAbstractListModel(parent)
    , m_context(context)
{
    connect(&m_context.selection(), &SelectionModel::changed, this, &EventModel::refresh);
    DocumentNotifier& notifier = m_context.project().notifier();
    connect(&notifier, &DocumentNotifier::eventsChanged, this,
            [this](ObjectId changed)
            {
                if (const HmiObject* shown = object(); shown && shown->id() == changed)
                    refresh();
            });
    connect(&notifier, &DocumentNotifier::projectReset, this, &EventModel::refresh);
    // The pickers list tags and screens; their names change with the project.
    connect(&notifier, &DocumentNotifier::tagAdded, this, &EventModel::namesChanged);
    connect(&notifier, &DocumentNotifier::tagRemoved, this, &EventModel::namesChanged);
    connect(&notifier, &DocumentNotifier::tagChanged, this, &EventModel::namesChanged);
    connect(&notifier, &DocumentNotifier::screenAdded, this, &EventModel::namesChanged);
    connect(&notifier, &DocumentNotifier::screenRemoved, this, &EventModel::namesChanged);
    connect(&notifier, &DocumentNotifier::screenChanged, this, &EventModel::namesChanged);
    refresh();
}

HmiObject* EventModel::object() const
{
    const auto ids = m_context.selection().ids();
    const Screen* screen = m_context.screen();
    return ids.size() == 1 && screen ? screen->object(ids.front()) : nullptr;
}

const ActionList& EventModel::actions() const
{
    static const ActionList kNone;
    const HmiObject* target = object();
    return target ? target->events().actions(m_event) : kNone;
}

void EventModel::refresh()
{
    beginResetModel();
    m_rows = actions();
    endResetModel();
    if (available() != m_available)
    {
        m_available = available();
        Q_EMIT availableChanged();
    }
    Q_EMIT namesChanged(); // the counts in the event names
}

void EventModel::setEvent(int event)
{
    if (event < 0 || event >= int(kEventKinds.size()) || event == int(m_event))
        return;
    m_event = kEventKinds[std::size_t(event)];
    refresh();
    Q_EMIT eventChanged();
}

QStringList EventModel::eventNames() const
{
    QStringList names;
    const HmiObject* target = object();
    for (const EventKind event : kEventKinds)
    {
        const std::size_t count = target ? target->events().actions(event).size() : 0;
        names << (count > 0 ? tr("%1 (%2)").arg(Events::displayName(event)).arg(count)
                            : Events::displayName(event));
    }
    return names;
}

QStringList EventModel::tagNames() const
{
    QStringList names;
    for (const Tag& tag : m_context.project().tags().tags())
        names << tag.name;
    return names;
}

QStringList EventModel::screenNames() const
{
    QStringList names;
    for (const auto& screen : m_context.project().screens())
    {
        if (screen->id() != m_context.screenId())
            names << screen->name();
    }
    return names;
}

int EventModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant EventModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Action& action = m_rows[std::size_t(index.row())];
    switch (role)
    {
    case KindRole:
        return int(action.kind);
    case TagRole:
        return action.tag;
    case ValueRole:
        return action.value;
    case ScreenRole:
        return action.screen;
    case FieldsRole:
        switch (action.kind)
        {
        case Action::Kind::SetTag:
        case Action::Kind::IncrementTag:
            return QStringLiteral("tagValue");
        case Action::Kind::ToggleTag:
            return QStringLiteral("tag");
        case Action::Kind::OpenScreen:
        case Action::Kind::ShowPopup:
            return QStringLiteral("screen");
        case Action::Kind::ClosePopup:
            return QStringLiteral("none");
        }
        return {};
    default:
        return {};
    }
}

QHash<int, QByteArray> EventModel::roleNames() const
{
    return {{KindRole, "kind"},
            {TagRole, "tag"},
            {ValueRole, "value"},
            {ScreenRole, "screen"},
            {FieldsRole, "fields"}};
}

void EventModel::change(ActionList after)
{
    const HmiObject* target = object();
    if (!target || after == target->events().actions(m_event))
        return;
    m_context.undoStack().push(new SetActionsCommand(m_context.project(), m_context.screenId(),
                                                     target->id(), m_event, std::move(after)));
}

template <typename Edit>
void EventModel::editRow(int row, Edit edit)
{
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return;
    ActionList after = actions();
    edit(after[std::size_t(row)]);
    change(std::move(after));
}

void EventModel::addAction(int kind)
{
    if (!object() || kind < 0 || kind >= int(Events::actionNames().size()))
        return;
    ActionList after = actions();
    Action action;
    action.kind = Action::Kind(kind);
    if (action.kind == Action::Kind::SetTag)
        action.value = QStringLiteral("1");
    else if (action.kind == Action::Kind::IncrementTag)
        action.value = QStringLiteral("1");
    after.push_back(std::move(action));
    change(std::move(after));
}

void EventModel::removeAction(int row)
{
    if (row < 0 || std::size_t(row) >= m_rows.size())
        return;
    ActionList after = actions();
    after.erase(after.begin() + row);
    change(std::move(after));
}

void EventModel::moveAction(int row, int delta)
{
    const int to = row + delta;
    if (row < 0 || to < 0 || std::size_t(row) >= m_rows.size() || std::size_t(to) >= m_rows.size())
        return;
    ActionList after = actions();
    std::swap(after[std::size_t(row)], after[std::size_t(to)]);
    change(std::move(after));
}

void EventModel::setKind(int row, int kind)
{
    if (kind < 0 || kind >= int(Events::actionNames().size()))
        return;
    editRow(row, [kind](Action& action) { action.kind = Action::Kind(kind); });
}

void EventModel::setTag(int row, const QString& tag)
{
    editRow(row, [&tag](Action& action) { action.tag = tag.trimmed(); });
}

void EventModel::setValue(int row, const QString& value)
{
    editRow(row, [&value](Action& action) { action.value = value.trimmed(); });
}

void EventModel::setScreen(int row, const QString& screen)
{
    editRow(row, [&screen](Action& action) { action.screen = screen; });
}

} // namespace hmi
