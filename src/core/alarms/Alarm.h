#pragma once

#include "core/Result.h"
#include "core/tags/Tag.h"

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace hmi
{

// How an alarm of a class is acknowledged, as the state machines of WinCC
// Unified alarm classes ("Alarm with simple acknowledgment", ...).
enum class AlarmStateMachine
{
    WithoutAcknowledgment,         // pending while the condition holds
    SimpleAcknowledgment,          // pending until acknowledged and gone
    AcknowledgmentAndConfirmation, // also confirmed after it has gone
};

// The status an operator sees, with WinCC Unified's texts. Normal alarms
// are no longer pending.
enum class AlarmStatus
{
    Normal,
    Incoming,
    IncomingOutgoing,
    IncomingAcknowledged,
    IncomingOutgoingAcknowledged, // acknowledged after it went; to confirm
    IncomingAcknowledgedOutgoing, // went after it was acknowledged; to confirm
};
inline constexpr int kAlarmStatusCount = 6;

// Text colour, background and flashing of alarms in one status.
struct AlarmColors
{
    QColor text;
    QColor background;
    bool flashing = false;
    // By value: the same colour in another model (HSV, RGB) is equal.
    friend bool operator==(const AlarmColors& a, const AlarmColors& b)
    {
        const auto same = [](const QColor& x, const QColor& y)
        {
            return x.isValid() == y.isValid() && (!x.isValid() || x.rgba() == y.rgba());
        };
        return same(a.text, b.text) && same(a.background, b.background) && a.flashing == b.flashing;
    }
};

// The appearance, acknowledgment model and priority shared by alarms, as a
// WinCC Unified alarm class. The predefined classes cannot be renamed,
// removed or given another state machine; their colours and priority can
// be changed.
struct AlarmClass
{
    // The four statuses colours are configured for, in this order.
    enum ColorState
    {
        IncomingColors,
        IncomingOutgoingColors,
        IncomingAcknowledgedColors,
        IncomingOutgoingAcknowledgedColors,
        ColorStateCount,
    };

    QString name;
    AlarmStateMachine stateMachine = AlarmStateMachine::SimpleAcknowledgment;
    int priority = 8; // 0 (lowest) to 16
    std::array<AlarmColors, ColorStateCount> colors{};
    bool predefined = false;
    QJsonObject preserved;

    [[nodiscard]] const AlarmColors& colorsFor(AlarmStatus status) const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<AlarmClass> fromJson(const QJsonObject& json);
    // Alarm, Warning, Notification, AlarmWithReset, WarningWithReset.
    [[nodiscard]] static std::vector<AlarmClass> predefinedClasses();
    friend bool operator==(const AlarmClass&, const AlarmClass&) = default;
};

// One alarm of the project: a discrete alarm watches a bit of a tag, an
// analog alarm compares a tag with a limit.
struct Alarm
{
    enum class Kind
    {
        Discrete,
        Analog,
    };
    // Analog alarm modes, as in WinCC Unified.
    enum class Mode
    {
        Less,
        Greater,
        Equal,
        NotEqual,
        LessOrEqual,
        GreaterOrEqual,
    };

    int id = 1; // the alarm number, unique in the project
    QString name;
    Kind kind = Kind::Discrete;
    // May show values in braces, replaced at runtime: an alarm parameter
    // by number, "{1}", or a tag by name, "{Level}", with an optional
    // format, "{1:6.2}" (see Alarms::formatText).
    QString text;
    QString alarmClass = QStringLiteral("Alarm");
    QString infoText;
    QString origin;
    QString tag; // the trigger tag
    // Discrete: the bit of the trigger tag (0 for a Bool tag), and whether
    // the alarm comes when it goes to 0 instead of 1.
    int bit = 0;
    bool fallingEdge = false;
    // Analog
    Mode mode = Mode::Greater;
    double limit = 0.0;
    // The alarm parameters (associated values): tags whose values the text
    // shows as {1} to {10}.
    QStringList parameters;
    QJsonObject preserved;

    static constexpr int kMaxParameters = 10;

    // Whether a tag value raises the alarm.
    [[nodiscard]] bool isActive(double value) const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<Alarm> fromJson(const QJsonObject& json);
    friend bool operator==(const Alarm&, const Alarm&) = default;
};

// The alarm settings of the runtime, as WinCC Unified's Runtime settings >
// Alarms: the status texts operators see, and how long a shelved alarm
// stays shelved.
struct AlarmSettings
{
    // Per AlarmStatus; empty means the default text.
    std::array<QString, kAlarmStatusCount> statusTexts{};
    // Seconds; 0 keeps shelved alarms shelved until unshelved.
    int shelvingTimeout = 3600;
    QJsonObject preserved;

    [[nodiscard]] QString statusText(AlarmStatus status) const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static AlarmSettings fromJson(const QJsonObject& json);
    friend bool operator==(const AlarmSettings&, const AlarmSettings&) = default;
};

// The project's alarms and alarm classes.
struct AlarmConfiguration
{
    std::vector<AlarmClass> classes = AlarmClass::predefinedClasses();
    std::vector<Alarm> alarms;
    AlarmSettings settings;

    [[nodiscard]] const AlarmClass* findClass(const QString& name) const;
    [[nodiscard]] const Alarm* findAlarm(int id) const;
    // Unique alarm numbers and class names, known classes, predefined
    // classes present and unchanged in name and state machine.
    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] int nextId() const;
    friend bool operator==(const AlarmConfiguration&, const AlarmConfiguration&) = default;
};

namespace Alarms
{
// Translated names, in enum order.
[[nodiscard]] QStringList statusNames();       // AlarmStatus
[[nodiscard]] QStringList stateMachineNames(); // AlarmStateMachine
[[nodiscard]] QStringList modeNames();         // Alarm::Mode
[[nodiscard]] QString statusName(AlarmStatus status);
// The key of a status in files, "incomingOutgoing", and back.
[[nodiscard]] QString statusKey(AlarmStatus status);
[[nodiscard]] std::optional<AlarmStatus> statusFromKey(const QString& key);

// The value of a tag, or nothing for an unknown tag.
using TagLookup = std::function<std::optional<TagValue>(const QString& tag)>;
// An alarm's text with its values filled in. {n} shows alarm parameter n
// (1 to 10), {Name} the tag Name. A format may follow a colon:
// [0][-][width][.decimals] — leading zeros, left alignment, the least
// width, decimals of numbers (reals show 1 by default). Unknown fields stay
// as written. An empty text shows the alarm's name.
[[nodiscard]] QString formatText(const Alarm& alarm, const TagLookup& value);
} // namespace Alarms

} // namespace hmi
