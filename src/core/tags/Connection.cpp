#include "core/tags/Connection.h"

#include <QRegularExpression>

namespace hmi
{

Result<void> Connection::validate() const
{
    static const QRegularExpression hostPattern(QStringLiteral("^[A-Za-z0-9_.:-]+$"));
    static const QRegularExpression routePattern(QStringLiteral("^\\d+(,\\d+)*$"));
    if (id.isEmpty() || name.trimmed().isEmpty())
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("A connection needs an id and name"));
    if (driver != u"s7" && driver != u"ab-eip" && driver != u"modbus-tcp")
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Unknown PLC driver '%1'").arg(driver));
    if (!hostPattern.match(host).hasMatch())
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Enter a PLC host name or IP address"));
    if (rack < 0 || rack > 7 || slot < 0 || slot > 31 || unit < 0 || unit > 255)
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("PLC rack, slot or unit is out of range"));
    if (!routePattern.match(path).hasMatch())
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("Use a CIP route such as 1,0"));
    return {};
}

QJsonObject Connection::toJson() const
{
    QJsonObject json = preserved;
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("driver"), driver);
    json.insert(QStringLiteral("host"), host);
    json.insert(QStringLiteral("rack"), rack);
    json.insert(QStringLiteral("slot"), slot);
    json.insert(QStringLiteral("path"), path);
    json.insert(QStringLiteral("unit"), unit);
    json.insert(QStringLiteral("library"), library);
    return json;
}

Result<Connection> Connection::fromJson(const QJsonObject& json)
{
    for (const char* field : {"rack", "slot", "unit"})
    {
        const QJsonValue value = json.value(QLatin1StringView(field));
        if (!value.isUndefined() && (!value.isDouble() || value.toDouble() != value.toInt(-1)))
            return makeError(
                ErrorCode::BadFormat,
                QStringLiteral("PLC %1 must be an integer").arg(QLatin1StringView(field)));
    }
    Connection connection;
    connection.id = json.value(QStringLiteral("id")).toString();
    connection.name = json.value(QStringLiteral("name")).toString();
    connection.driver = json.value(QStringLiteral("driver")).toString();
    connection.host = json.value(QStringLiteral("host")).toString();
    connection.rack = json.value(QStringLiteral("rack")).toInt(0);
    connection.slot = json.value(QStringLiteral("slot")).toInt(1);
    connection.path = json.value(QStringLiteral("path")).toString(QStringLiteral("1,0"));
    connection.unit = json.value(QStringLiteral("unit")).toInt(1);
    connection.library = json.value(QStringLiteral("library")).toString();
    if (auto checked = connection.validate(); !checked)
        return tl::unexpected(checked.error());
    connection.preserved = json;
    for (const char* key :
         {"id", "name", "driver", "host", "rack", "slot", "path", "unit", "library"})
        connection.preserved.remove(QLatin1StringView(key));
    return connection;
}

} // namespace hmi
