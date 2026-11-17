#pragma once

#include "core/Result.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <array>
#include <vector>

namespace hmi
{

// What an operator does to an object at runtime, as in the events of a
// WinCC screen object.
enum class EventKind
{
    Click,   // pressed and released on the object
    Press,   // pressed
    Release, // released (also when the press is cancelled)
};

inline constexpr std::array kEventKinds{EventKind::Click, EventKind::Press, EventKind::Release};

// One step of an event's action list (the "system functions" of WinCC):
// tag operations and screen navigation.
struct Action
{
    enum class Kind
    {
        SetTag,       // tag := value
        ToggleTag,    // a Bool tag := !tag
        IncrementTag, // tag := tag + value (negative to decrease)
        OpenScreen,   // show another screen
    };

    Kind kind = Kind::SetTag;
    QString tag;    // tag actions
    QString value;  // SetTag: the value; IncrementTag: the step
    QString screen; // OpenScreen: the screen's name

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<Action> fromJson(const QJsonObject& json);
    // Whether the action names a tag (for "where used" and renaming).
    [[nodiscard]] bool usesTag() const noexcept { return kind != Kind::OpenScreen; }

    friend bool operator==(const Action&, const Action&) = default;
};

using ActionList = std::vector<Action>;

// The action lists of an object, one per event kind.
class EventTable
{
public:
    [[nodiscard]] const ActionList& actions(EventKind event) const
    {
        return m_lists[std::size_t(event)];
    }
    void setActions(EventKind event, ActionList actions)
    {
        m_lists[std::size_t(event)] = std::move(actions);
    }
    [[nodiscard]] bool isEmpty() const;

    // {"click": [...], "press": [...]}; events without actions are left out.
    [[nodiscard]] QJsonObject toJson() const;
    // Actions this version cannot read are returned in `unread`, by event
    // key, to be written back unchanged.
    [[nodiscard]] static EventTable fromJson(const QJsonObject& json, QJsonObject* unread);

    friend bool operator==(const EventTable&, const EventTable&) = default;

private:
    std::array<ActionList, kEventKinds.size()> m_lists;
};

namespace Events
{
// "click", "press", "release": the keys in files.
[[nodiscard]] QString key(EventKind event);
// "Click", "Press", "Release", translated.
[[nodiscard]] QString displayName(EventKind event);
// "Set tag", "Toggle tag", ...: translated, in Action::Kind order.
[[nodiscard]] QStringList actionNames();
} // namespace Events

} // namespace hmi
