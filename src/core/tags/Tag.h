#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QString>
#include <QVariant>

#include <optional>
#include <variant>

namespace hmi
{

// The value of a tag at one moment. Empty (monostate) until a value is known.
using TagValue = std::variant<std::monostate, bool, qint64, double, QString>;

// Conversions used by bindings and the tags panel.
namespace TagValues
{
[[nodiscard]] std::optional<double> toNumber(const TagValue& value);
[[nodiscard]] bool toBool(const TagValue& value);
[[nodiscard]] QString toText(const TagValue& value);
[[nodiscard]] QVariant toVariant(const TagValue& value);
} // namespace TagValues

// A process variable an HMI screen shows or sets (ARCHITECTURE.md section
// 5.3). Without a PLC connection (until phase 8) tags are simulated: the
// simulation mode says how the value moves between minimum and maximum.
struct Tag
{
    enum class Type
    {
        Bool,
        Int,
        Real,
        String,
    };

    enum class Simulation
    {
        None,   // keeps its value; set by hand while simulating
        Ramp,   // minimum to maximum over the period, then again
        Sine,   // between minimum and maximum with the period
        Random, // a new random value every period / 10
        Toggle, // false and true, half a period each (any type)
    };

    QString name;
    Type type = Type::Real;
    QString unit;
    QString comment;
    double minimum = 0.0;
    double maximum = 100.0;
    Simulation simulation = Simulation::None;
    double period = 10.0; // seconds
    TagValue initial;
    // Where the value comes from (phase 8: a PLC connection and address).
    QString connection;
    QString address;
    // Keys this version does not know, written back unchanged.
    QJsonObject preserved;

    // The value a new tag has before the simulation or a driver sets one.
    [[nodiscard]] TagValue startValue() const;
    // `value` converted to this tag's type, or empty if it cannot be.
    [[nodiscard]] TagValue converted(const TagValue& value) const;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<Tag> fromJson(const QJsonObject& json);

    // Stable keys for files and the tags panel.
    [[nodiscard]] static QString typeKey(Type type);
    [[nodiscard]] static std::optional<Type> typeFromKey(QStringView key);
    [[nodiscard]] static QString simulationKey(Simulation simulation);
    [[nodiscard]] static std::optional<Simulation> simulationFromKey(QStringView key);

    // A name is valid when it is not empty and has no leading or trailing
    // spaces; names are compared without regard to case.
    [[nodiscard]] static bool isValidName(const QString& name);

    friend bool operator==(const Tag&, const Tag&) = default;
};

} // namespace hmi
