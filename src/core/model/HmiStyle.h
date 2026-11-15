#pragma once

#include "core/property/PropertyDescriptor.h"

#include <QColor>
#include <QJsonObject>
#include <QString>

namespace hmi
{

class HmiObject;

// The project's shared look for HMI objects: colours by role and a font
// family. New objects take it when they are placed; "Apply project style"
// gives it to existing ones. Saved in project.json.
struct HmiStyle
{
    QColor face{Qt::white};
    QColor line{0x9A, 0xA1, 0xAB};
    QColor text{0x1F, 0x23, 0x28};
    QColor accent{0x3B, 0x82, 0xF6};
    // Empty: keep each object's own family.
    QString fontFamily;

    // Sets every property of `object` that has a style role. Returns
    // whether anything changed.
    bool applyTo(HmiObject& object) const;

    [[nodiscard]] QJsonObject toJson() const;
    // Missing or invalid entries keep their defaults.
    [[nodiscard]] static HmiStyle fromJson(const QJsonObject& json);

    friend bool operator==(const HmiStyle&, const HmiStyle&) = default;
};

} // namespace hmi
