#include "core/model/Events.h"

#include <QCoreApplication>

#include <algorithm>

namespace hmi
{

namespace
{

constexpr const char* kActionKeys[] = {"setTag",     "toggleTag", "incrementTag",
                                       "openScreen", "showPopup", "closePopup"};

QString tr(const char* text)
{
    return QCoreApplication::translate("hmi::Events", text);
}

} // namespace

QJsonObject Action::toJson() const
{
    QJsonObject json{{QStringLiteral("action"), QLatin1StringView(kActionKeys[int(kind)])}};
    if (usesTag())
        json.insert(QStringLiteral("tag"), tag);
    if (kind == Kind::SetTag || kind == Kind::IncrementTag)
        json.insert(QStringLiteral("value"), value);
    if (usesScreen())
        json.insert(QStringLiteral("screen"), screen);
    return json;
}

Result<Action> Action::fromJson(const QJsonObject& json)
{
    const QString key = json.value(QStringLiteral("action")).toString();
    const auto* found = std::ranges::find_if(kActionKeys, [&key](const char* candidate)
                                             { return key == QLatin1StringView(candidate); });
    if (found == std::end(kActionKeys))
        return makeError(ErrorCode::BadFormat, QStringLiteral("Unknown action '%1'").arg(key));
    Action action;
    action.kind = Kind(found - std::begin(kActionKeys));
    action.tag = json.value(QStringLiteral("tag")).toString();
    action.value = json.value(QStringLiteral("value")).toString();
    action.screen = json.value(QStringLiteral("screen")).toString();
    return action;
}

bool EventTable::isEmpty() const
{
    return std::ranges::all_of(m_lists, &ActionList::empty);
}

QJsonObject EventTable::toJson() const
{
    QJsonObject json;
    for (const EventKind event : kEventKinds)
    {
        const ActionList& list = actions(event);
        if (list.empty())
            continue;
        QJsonArray array;
        for (const Action& action : list)
            array.append(action.toJson());
        json.insert(Events::key(event), array);
    }
    return json;
}

EventTable EventTable::fromJson(const QJsonObject& json, QJsonObject* unread)
{
    EventTable table;
    QJsonObject rest = json;
    for (const EventKind event : kEventKinds)
    {
        const QString key = Events::key(event);
        if (!json.contains(key))
            continue;
        rest.remove(key);
        ActionList list;
        QJsonArray kept;
        for (const QJsonValue& entry : json.value(key).toArray())
        {
            if (auto action = Action::fromJson(entry.toObject()))
                list.push_back(std::move(*action));
            else
                kept.append(entry);
        }
        table.setActions(event, std::move(list));
        if (!kept.isEmpty())
            rest.insert(key, kept);
    }
    if (unread)
        *unread = rest;
    return table;
}

namespace Events
{

QString key(EventKind event)
{
    switch (event)
    {
    case EventKind::Click:
        return QStringLiteral("click");
    case EventKind::Press:
        return QStringLiteral("press");
    case EventKind::Release:
        return QStringLiteral("release");
    }
    return {};
}

QString displayName(EventKind event)
{
    switch (event)
    {
    case EventKind::Click:
        return tr("Click");
    case EventKind::Press:
        return tr("Press");
    case EventKind::Release:
        return tr("Release");
    }
    return {};
}

QStringList actionNames()
{
    return {tr("Set tag"),     tr("Toggle tag"), tr("Increment tag"),
            tr("Open screen"), tr("Show popup"), tr("Close popup")};
}

} // namespace Events

} // namespace hmi
