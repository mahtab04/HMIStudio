#pragma once

#include "core/Result.h"

#include <QJsonObject>
#include <QString>

namespace hmi
{

// A named, independently configured PLC endpoint. Tags reference the stable
// id so renaming a connection does not change their addresses or bindings.
struct Connection
{
    QString id;
    QString name;
    QString driver = QStringLiteral("s7");
    QString host;
    int rack = 0;
    int slot = 1;
    QString path = QStringLiteral("1,0");
    int unit = 1;
    QString library;
    QJsonObject preserved;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static Result<Connection> fromJson(const QJsonObject& json);
    friend bool operator==(const Connection&, const Connection&) = default;
};

} // namespace hmi
