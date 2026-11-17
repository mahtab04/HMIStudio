#include "core/alarms/Alarm.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace hmi
{

namespace
{

QString tr(const char* text)
{
    return QCoreApplication::translate("hmi::Alarms", text);
}

constexpr const char* kStateMachineKeys[] = {"withoutAcknowledgment", "simpleAcknowledgment",
                                             "acknowledgmentAndConfirmation"};
constexpr const char* kColorKeys[] = {"incoming", "incomingOutgoing", "incomingAcknowledged",
                                      "incomingOutgoingAcknowledged"};
constexpr const char* kModeKeys[] = {"less",     "greater",     "equal",
                                     "notEqual", "lessOrEqual", "greaterOrEqual"};

template <std::size_t N>
int keyIndex(const char* const (&keys)[N], const QString& key)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        if (key == QLatin1StringView(keys[i]))
            return int(i);
    }
    return -1;
}

// Colours not set are left out, so they stay unset when read back.
QJsonObject colorsToJson(const AlarmColors& colors)
{
    QJsonObject json{{QStringLiteral("flashing"), colors.flashing}};
    if (colors.text.isValid())
        json.insert(QStringLiteral("text"), colors.text.name());
    if (colors.background.isValid())
        json.insert(QStringLiteral("background"), colors.background.name());
    return json;
}

AlarmColors colorsFromJson(const QJsonObject& json, const AlarmColors& fallback)
{
    AlarmColors colors = fallback;
    if (const QColor text(json.value(QStringLiteral("text")).toString()); text.isValid())
        colors.text = text;
    if (const QColor background(json.value(QStringLiteral("background")).toString());
        background.isValid())
        colors.background = background;
    colors.flashing = json.value(QStringLiteral("flashing")).toBool(fallback.flashing);
    return colors;
}

// The predefined classes, with WinCC-like colours: alarms red (flashing
// while incoming), warnings yellow, notifications blue-grey.
AlarmClass makeClass(const char* name, AlarmStateMachine machine, int priority, QColor incoming,
                     QColor incomingText, bool flashing)
{
    AlarmClass c;
    c.name = QString::fromLatin1(name);
    c.stateMachine = machine;
    c.priority = priority;
    c.predefined = true;
    c.colors[AlarmClass::IncomingColors] = {incomingText, incoming, flashing};
    // Gone but not acknowledged: the same colour, lighter.
    c.colors[AlarmClass::IncomingOutgoingColors] = {QColor(0x1F, 0x23, 0x28), incoming.lighter(160),
                                                    false};
    // Acknowledged while still active: darker, steady.
    c.colors[AlarmClass::IncomingAcknowledgedColors] = {QColor(Qt::white), incoming.darker(150),
                                                        false};
    c.colors[AlarmClass::IncomingOutgoingAcknowledgedColors] = {QColor(0x1F, 0x23, 0x28),
                                                                QColor(0xD9, 0xDC, 0xE1), false};
    return c;
}

} // namespace

const AlarmColors& AlarmClass::colorsFor(AlarmStatus status) const
{
    switch (status)
    {
    case AlarmStatus::IncomingOutgoing:
        return colors[IncomingOutgoingColors];
    case AlarmStatus::IncomingAcknowledged:
        return colors[IncomingAcknowledgedColors];
    case AlarmStatus::IncomingOutgoingAcknowledged:
    case AlarmStatus::IncomingAcknowledgedOutgoing:
        return colors[IncomingOutgoingAcknowledgedColors];
    case AlarmStatus::Normal:
    case AlarmStatus::Incoming:
        break;
    }
    return colors[IncomingColors];
}

QJsonObject AlarmClass::toJson() const
{
    QJsonObject json = preserved;
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("stateMachine"),
                QLatin1StringView(kStateMachineKeys[int(stateMachine)]));
    json.insert(QStringLiteral("priority"), priority);
    QJsonObject colorJson;
    for (int i = 0; i < ColorStateCount; ++i)
        colorJson.insert(QLatin1StringView(kColorKeys[i]), colorsToJson(colors[std::size_t(i)]));
    json.insert(QStringLiteral("colors"), colorJson);
    return json;
}

Result<AlarmClass> AlarmClass::fromJson(const QJsonObject& json)
{
    AlarmClass c;
    c.name = json.value(QStringLiteral("name")).toString().trimmed();
    if (c.name.isEmpty())
        return makeError(ErrorCode::BadFormat, QStringLiteral("An alarm class needs a name"));
    const int machine =
        keyIndex(kStateMachineKeys, json.value(QStringLiteral("stateMachine")).toString());
    if (machine < 0)
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Alarm class %1 has an unknown state machine").arg(c.name));
    c.stateMachine = AlarmStateMachine(machine);
    c.priority = std::clamp(json.value(QStringLiteral("priority")).toInt(8), 0, 16);
    // Classes named like a predefined one start from its colours.
    const auto defaults = predefinedClasses();
    const auto known = std::ranges::find(defaults, c.name, &AlarmClass::name);
    if (known != defaults.end())
    {
        c.colors = known->colors;
        c.predefined = true;
    }
    const QJsonObject colorJson = json.value(QStringLiteral("colors")).toObject();
    for (int i = 0; i < ColorStateCount; ++i)
        c.colors[std::size_t(i)] = colorsFromJson(
            colorJson.value(QLatin1StringView(kColorKeys[i])).toObject(), c.colors[std::size_t(i)]);
    c.preserved = json;
    for (const char* key : {"name", "stateMachine", "priority", "colors"})
        c.preserved.remove(QLatin1StringView(key));
    return c;
}

std::vector<AlarmClass> AlarmClass::predefinedClasses()
{
    using M = AlarmStateMachine;
    const QColor red(0xD6, 0x45, 0x41);
    const QColor yellow(0xF2, 0xC2, 0x30);
    const QColor blue(0x5B, 0x8D, 0xC9);
    return {
        makeClass("Alarm", M::SimpleAcknowledgment, 12, red, Qt::white, true),
        makeClass("Warning", M::SimpleAcknowledgment, 8, yellow, QColor(0x1F, 0x23, 0x28), false),
        makeClass("Notification", M::WithoutAcknowledgment, 4, blue, Qt::white, false),
        makeClass("AlarmWithReset", M::AcknowledgmentAndConfirmation, 12, red, Qt::white, true),
        makeClass("WarningWithReset", M::AcknowledgmentAndConfirmation, 8, yellow,
                  QColor(0x1F, 0x23, 0x28), false),
    };
}

bool Alarm::isActive(double value) const
{
    if (kind == Kind::Discrete)
    {
        const auto bits = static_cast<quint64>(std::llround(value));
        const bool set = (bits >> std::clamp(bit, 0, 63)) & 1U;
        return fallingEdge ? !set : set;
    }
    switch (mode)
    {
    case Mode::Less:
        return value < limit;
    case Mode::Greater:
        return value > limit;
    case Mode::Equal:
        return value == limit;
    case Mode::NotEqual:
        return value != limit;
    case Mode::LessOrEqual:
        return value <= limit;
    case Mode::GreaterOrEqual:
        return value >= limit;
    }
    return false;
}

QJsonObject Alarm::toJson() const
{
    QJsonObject json = preserved;
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("kind"),
                kind == Kind::Discrete ? QStringLiteral("discrete") : QStringLiteral("analog"));
    json.insert(QStringLiteral("text"), text);
    json.insert(QStringLiteral("class"), alarmClass);
    json.insert(QStringLiteral("infoText"), infoText);
    json.insert(QStringLiteral("origin"), origin);
    json.insert(QStringLiteral("tag"), tag);
    if (kind == Kind::Discrete)
    {
        json.insert(QStringLiteral("bit"), bit);
        json.insert(QStringLiteral("fallingEdge"), fallingEdge);
    }
    else
    {
        json.insert(QStringLiteral("mode"), QLatin1StringView(kModeKeys[int(mode)]));
        json.insert(QStringLiteral("limit"), limit);
    }
    return json;
}

Result<Alarm> Alarm::fromJson(const QJsonObject& json)
{
    Alarm a;
    a.id = json.value(QStringLiteral("id")).toInt(0);
    if (a.id < 1)
        return makeError(ErrorCode::BadFormat, QStringLiteral("An alarm needs a number from 1"));
    const QString kindKey = json.value(QStringLiteral("kind")).toString();
    if (kindKey != u"discrete" && kindKey != u"analog")
        return makeError(
            ErrorCode::BadFormat,
            QStringLiteral("Alarm %1 has an unknown kind '%2'").arg(a.id).arg(kindKey));
    a.kind = kindKey == u"discrete" ? Kind::Discrete : Kind::Analog;
    a.name = json.value(QStringLiteral("name")).toString();
    a.text = json.value(QStringLiteral("text")).toString();
    a.alarmClass = json.value(QStringLiteral("class")).toString(QStringLiteral("Alarm"));
    a.infoText = json.value(QStringLiteral("infoText")).toString();
    a.origin = json.value(QStringLiteral("origin")).toString();
    a.tag = json.value(QStringLiteral("tag")).toString();
    a.bit = json.value(QStringLiteral("bit")).toInt(0);
    a.fallingEdge = json.value(QStringLiteral("fallingEdge")).toBool(false);
    if (a.kind == Kind::Analog)
    {
        const int mode = keyIndex(kModeKeys, json.value(QStringLiteral("mode")).toString());
        if (mode < 0)
            return makeError(ErrorCode::BadFormat,
                             QStringLiteral("Alarm %1 has an unknown limit mode").arg(a.id));
        a.mode = Mode(mode);
        a.limit = json.value(QStringLiteral("limit")).toDouble();
    }
    if (a.bit < 0 || a.bit > 63 || !std::isfinite(a.limit))
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Alarm %1 has an invalid bit or limit").arg(a.id));
    a.preserved = json;
    for (const char* key : {"id", "name", "kind", "text", "class", "infoText", "origin", "tag",
                            "bit", "fallingEdge", "mode", "limit"})
        a.preserved.remove(QLatin1StringView(key));
    return a;
}

const AlarmClass* AlarmConfiguration::findClass(const QString& name) const
{
    const auto it =
        std::ranges::find_if(classes, [&name](const AlarmClass& c)
                             { return c.name.compare(name, Qt::CaseInsensitive) == 0; });
    return it == classes.end() ? nullptr : &*it;
}

const Alarm* AlarmConfiguration::findAlarm(int id) const
{
    const auto it = std::ranges::find(alarms, id, &Alarm::id);
    return it == alarms.end() ? nullptr : &*it;
}

Result<void> AlarmConfiguration::validate() const
{
    QSet<QString> names;
    for (const AlarmClass& c : classes)
    {
        if (c.name.trimmed().isEmpty() || names.contains(c.name.toCaseFolded()))
            return makeError(ErrorCode::Duplicate,
                             tr("Alarm class names must be unique and not empty."));
        if (c.priority < 0 || c.priority > 16)
            return makeError(ErrorCode::OutOfRange, tr("Priorities go from 0 to 16."));
        names.insert(c.name.toCaseFolded());
    }
    for (const AlarmClass& fixed : AlarmClass::predefinedClasses())
    {
        const AlarmClass* c = findClass(fixed.name);
        if (!c || c->name != fixed.name || c->stateMachine != fixed.stateMachine)
            return makeError(ErrorCode::InvalidArgument,
                             tr("The predefined alarm class %1 cannot be removed or changed in "
                                "name or state machine.")
                                 .arg(fixed.name));
    }
    QSet<int> ids;
    for (const Alarm& a : alarms)
    {
        if (a.id < 1 || ids.contains(a.id))
            return makeError(
                ErrorCode::Duplicate,
                tr("Alarm number %1 is used twice; numbers must be unique.").arg(a.id));
        ids.insert(a.id);
        if (!findClass(a.alarmClass))
            return makeError(
                ErrorCode::NotFound,
                tr("Alarm %1 uses the unknown alarm class %2.").arg(a.id).arg(a.alarmClass));
        if (a.bit < 0 || a.bit > 63 || !std::isfinite(a.limit))
            return makeError(
                ErrorCode::OutOfRange,
                tr("Alarm %1: the bit goes from 0 to 63, the limit must be a number.").arg(a.id));
    }
    return {};
}

int AlarmConfiguration::nextId() const
{
    int id = 1;
    for (const Alarm& a : alarms)
        id = std::max(id, a.id + 1);
    return id;
}

namespace Alarms
{

QStringList statusNames()
{
    return {tr("Normal"),
            tr("Incoming"),
            tr("Incoming/Outgoing"),
            tr("Incoming/Acknowledged"),
            tr("Incoming/Outgoing/Acknowledged"),
            tr("Incoming/Acknowledged/Outgoing")};
}

QString statusName(AlarmStatus status)
{
    return statusNames().at(int(status));
}

QStringList stateMachineNames()
{
    return {tr("Alarm without acknowledgment"), tr("Alarm with simple acknowledgment"),
            tr("Alarm with acknowledgment and confirmation")};
}

QStringList modeNames()
{
    return {tr("Less"),      tr("Greater"),       tr("Equal"),
            tr("Not equal"), tr("Less or equal"), tr("Greater or equal")};
}

} // namespace Alarms

} // namespace hmi
