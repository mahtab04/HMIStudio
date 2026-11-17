#include "core/tags/Tag.h"

#include <QJsonValue>

#include <array>
#include <cmath>

namespace hmi
{

namespace
{

const auto kName = QStringLiteral("name");
const auto kType = QStringLiteral("type");
const auto kUnit = QStringLiteral("unit");
const auto kComment = QStringLiteral("comment");
const auto kMinimum = QStringLiteral("minimum");
const auto kMaximum = QStringLiteral("maximum");
const auto kSimulation = QStringLiteral("simulation");
const auto kPeriod = QStringLiteral("period");
const auto kInitial = QStringLiteral("initial");
const auto kConnection = QStringLiteral("connection");
const auto kAddress = QStringLiteral("address");

struct TypeName
{
    Tag::Type type;
    const char* key;
};
constexpr std::array kTypes{TypeName{Tag::Type::Bool, "bool"}, TypeName{Tag::Type::Int, "int"},
                            TypeName{Tag::Type::Real, "real"},
                            TypeName{Tag::Type::String, "string"}};

struct SimulationName
{
    Tag::Simulation simulation;
    const char* key;
};
constexpr std::array kSimulations{
    SimulationName{Tag::Simulation::None, "none"},
    SimulationName{Tag::Simulation::Ramp, "ramp"},
    SimulationName{Tag::Simulation::Sine, "sine"},
    SimulationName{Tag::Simulation::Random, "random"},
    SimulationName{Tag::Simulation::Toggle, "toggle"},
};

QJsonValue toJsonValue(const TagValue& value)
{
    if (const auto* b = std::get_if<bool>(&value))
        return *b;
    if (const auto* i = std::get_if<qint64>(&value))
        return double(*i);
    if (const auto* d = std::get_if<double>(&value))
        return *d;
    if (const auto* s = std::get_if<QString>(&value))
        return *s;
    return QJsonValue::Null;
}

TagValue fromJsonValue(const QJsonValue& json)
{
    if (json.isBool())
        return json.toBool();
    if (json.isDouble())
        return json.toDouble();
    if (json.isString())
        return json.toString();
    return {};
}

} // namespace

namespace TagValues
{

std::optional<double> toNumber(const TagValue& value)
{
    if (const auto* b = std::get_if<bool>(&value))
        return *b ? 1.0 : 0.0;
    if (const auto* i = std::get_if<qint64>(&value))
        return double(*i);
    if (const auto* d = std::get_if<double>(&value))
        return *d;
    if (const auto* s = std::get_if<QString>(&value))
    {
        bool ok = false;
        const double number = s->trimmed().toDouble(&ok);
        if (ok)
            return number;
    }
    return std::nullopt;
}

bool toBool(const TagValue& value)
{
    if (const auto* s = std::get_if<QString>(&value))
    {
        const QString text = s->trimmed().toLower();
        return text == u"true" || text == u"on" || text == u"1";
    }
    const auto number = toNumber(value);
    return number && *number != 0.0;
}

QString toText(const TagValue& value)
{
    if (const auto* b = std::get_if<bool>(&value))
        return *b ? QStringLiteral("true") : QStringLiteral("false");
    if (const auto* i = std::get_if<qint64>(&value))
        return QString::number(*i);
    if (const auto* d = std::get_if<double>(&value))
        return QString::number(*d, 'g', 10);
    if (const auto* s = std::get_if<QString>(&value))
        return *s;
    return {};
}

QVariant toVariant(const TagValue& value)
{
    if (const auto* b = std::get_if<bool>(&value))
        return *b;
    if (const auto* i = std::get_if<qint64>(&value))
        return *i;
    if (const auto* d = std::get_if<double>(&value))
        return *d;
    if (const auto* s = std::get_if<QString>(&value))
        return *s;
    return {};
}

} // namespace TagValues

TagValue Tag::converted(const TagValue& value) const
{
    if (std::holds_alternative<std::monostate>(value))
        return {};
    switch (type)
    {
    case Type::Bool:
        return TagValues::toBool(value);
    case Type::Int:
        if (const auto* integer = std::get_if<qint64>(&value))
            return *integer;
        if (const auto number = TagValues::toNumber(value))
        {
            if (std::isfinite(*number) && *number < 9223372036854775808.0 &&
                *number >= -9223372036854775808.0)
                return qint64(std::llround(*number));
        }
        return {};
    case Type::Real:
        if (const auto number = TagValues::toNumber(value))
            return *number;
        return {};
    case Type::String:
        return TagValues::toText(value);
    }
    return {};
}

TagValue Tag::startValue() const
{
    if (!std::holds_alternative<std::monostate>(initial))
        return converted(initial);
    switch (type)
    {
    case Type::Bool:
        return false;
    case Type::Int:
        return qint64(std::llround(minimum));
    case Type::Real:
        return minimum;
    case Type::String:
        return QString();
    }
    return {};
}

QString Tag::typeKey(Type t)
{
    for (const TypeName& entry : kTypes)
    {
        if (entry.type == t)
            return QString::fromLatin1(entry.key);
    }
    return {};
}

std::optional<Tag::Type> Tag::typeFromKey(QStringView key)
{
    for (const TypeName& entry : kTypes)
    {
        if (key.compare(QLatin1StringView(entry.key), Qt::CaseInsensitive) == 0)
            return entry.type;
    }
    return std::nullopt;
}

QString Tag::simulationKey(Simulation s)
{
    for (const SimulationName& entry : kSimulations)
    {
        if (entry.simulation == s)
            return QString::fromLatin1(entry.key);
    }
    return {};
}

std::optional<Tag::Simulation> Tag::simulationFromKey(QStringView key)
{
    for (const SimulationName& entry : kSimulations)
    {
        if (key.compare(QLatin1StringView(entry.key), Qt::CaseInsensitive) == 0)
            return entry.simulation;
    }
    return std::nullopt;
}

bool Tag::isValidName(const QString& name)
{
    return !name.isEmpty() && name.trimmed() == name;
}

QJsonObject Tag::toJson() const
{
    QJsonObject json = preserved;
    json.insert(kName, name);
    json.insert(kType, typeKey(type));
    json.insert(kUnit, unit);
    json.insert(kComment, comment);
    json.insert(kMinimum, minimum);
    json.insert(kMaximum, maximum);
    json.insert(kSimulation, simulationKey(simulation));
    json.insert(kPeriod, period);
    json.insert(kInitial, toJsonValue(initial));
    json.insert(kConnection, connection);
    json.insert(kAddress, address);
    json.insert(QStringLiteral("table"), table);
    json.insert(QStringLiteral("acquisitionMs"), acquisitionMs);
    // Written only when on, so projects without limits save as before.
    if (limitInput)
        json.insert(QStringLiteral("limitInput"), true);
    return json;
}

Result<Tag> Tag::fromJson(const QJsonObject& json)
{
    Tag tag;
    tag.name = json.value(kName).toString();
    if (!isValidName(tag.name))
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Tag name '%1' is not valid").arg(tag.name));
    const auto type = typeFromKey(json.value(kType).toString(QStringLiteral("real")));
    if (!type)
        return makeError(ErrorCode::BadFormat, QStringLiteral("Tag '%1' has an unknown type '%2'")
                                                   .arg(tag.name, json.value(kType).toString()));
    tag.type = *type;
    tag.unit = json.value(kUnit).toString();
    tag.comment = json.value(kComment).toString();
    tag.minimum = json.value(kMinimum).toDouble(0.0);
    tag.maximum = json.value(kMaximum).toDouble(100.0);
    tag.simulation =
        simulationFromKey(json.value(kSimulation).toString()).value_or(Simulation::None);
    tag.period = json.value(kPeriod).toDouble(10.0);
    tag.initial = tag.converted(fromJsonValue(json.value(kInitial)));
    tag.connection = json.value(kConnection).toString();
    tag.address = json.value(kAddress).toString();
    const QJsonValue cycle = json.value(QStringLiteral("acquisitionMs"));
    if ((!cycle.isUndefined() && (!cycle.isDouble() || cycle.toDouble() != cycle.toInt(-1))) ||
        (json.contains(QStringLiteral("table")) && !json.value(QStringLiteral("table")).isString()))
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Invalid tag table or acquisition cycle"));
    tag.table = json.value(QStringLiteral("table")).toString(QStringLiteral("Default"));
    tag.acquisitionMs = json.value(QStringLiteral("acquisitionMs")).toInt(500);
    const QJsonValue limit = json.value(QStringLiteral("limitInput"));
    if (!limit.isUndefined() && !limit.isBool())
        return makeError(ErrorCode::BadFormat, QStringLiteral("limitInput must be true or false"));
    tag.limitInput = limit.toBool(false);
    if (tag.table.trimmed().isEmpty() || tag.acquisitionMs < 100 || tag.acquisitionMs > 60000)
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Invalid tag table or acquisition cycle"));
    if (!(tag.period > 0.0) || !std::isfinite(tag.minimum) || !std::isfinite(tag.maximum))
        return makeError(ErrorCode::BadFormat,
                         QStringLiteral("Tag '%1' has an invalid period or limits").arg(tag.name));

    QJsonObject rest = json;
    for (const QString& key : {kName, kType, kUnit, kComment, kMinimum, kMaximum, kSimulation,
                               kPeriod, kInitial, kConnection, kAddress, QStringLiteral("table"),
                               QStringLiteral("acquisitionMs"), QStringLiteral("limitInput")})
        rest.remove(key);
    tag.preserved = rest;
    return tag;
}

} // namespace hmi
