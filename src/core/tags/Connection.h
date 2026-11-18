#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QString>

namespace hmi
{

// A named PLC: its type (driver), IP address and, rarely needed, its port
// and protocol details. Tags reference the stable id so renaming a
// connection does not change their addresses or bindings.
struct Connection
{
    QString id;
    QString name;
    QString driver = QStringLiteral("s7"); // "s7", "modbus-tcp", "ab-eip"
    QString host;
    int port = 0; // 0: the protocol's port (S7 102, Modbus 502, EtherNet/IP 44818)
    int rack = 0;
    int slot = 1;
    QString path = QStringLiteral("1,0");
    int unit = 1;
    QString library;
    QJsonObject preserved;

    [[nodiscard]] Result<void> validate() const;
    // The port used: `port`, or the protocol's.
    [[nodiscard]] int effectivePort() const;
    [[nodiscard]] static int defaultPort(const QString& driver);
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<Connection> fromJson(const QJsonObject& json);
    friend bool operator==(const Connection&, const Connection&) = default;
};

} // namespace hmi
