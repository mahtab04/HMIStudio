#pragma once

// Checks every HmiObject type must pass. Each object test calls these from
// its own slots; they report through QVERIFY and return early on failure.

#include "core/model/HmiObject.h"
#include "core/model/PaintContext.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QImage>
#include <QPainter>
#include <QSet>
#include <QTest>

namespace hmi::test
{

// Descriptor table is well formed and a new object holds every default.
inline void checkDefaults(const HmiObject& object)
{
    QSet<PropertyId> seen;
    for (const PropertyDescriptor& d : object.descriptors())
    {
        QVERIFY2(d.key && d.displayName && d.category, "descriptor text missing");
        QCOMPARE(PropertyId::fromKey(QLatin1StringView(d.key)), d.id);
        QVERIFY2(!seen.contains(d.id), d.key);
        seen.insert(d.id);

        const auto stored = object.property(d.id);
        QVERIFY(stored.has_value());
        QVERIFY2(*stored == d.defaultValue, d.key);
        QVERIFY2(validate(d, d.defaultValue).has_value(), d.key);
    }
    for (const PropertyId common : {Prop::Name, Prop::X, Prop::Y, Prop::Width, Prop::Height,
                                    Prop::Rotation, Prop::Visible, Prop::Locked})
    {
        QVERIFY(seen.contains(common));
    }
}

// clone() is an exact copy; cloneWithNewId() differs only in id; both are
// independent of the original.
inline void checkClone(HmiObject& object)
{
    QVERIFY(object.setProperty(Prop::Name, QStringLiteral("original")));
    QVERIFY(object.setProperty(Prop::X, 12.5));

    const std::unique_ptr<HmiObject> copy = object.clone();
    QVERIFY(copy);
    QCOMPARE(copy->typeId(), object.typeId());
    QCOMPARE(copy->id(), object.id());
    QVERIFY(copy->hasSameProperties(object));

    const std::unique_ptr<HmiObject> fresh = object.cloneWithNewId();
    QVERIFY(fresh->id() != object.id());
    QVERIFY(fresh->hasSameProperties(object));

    QVERIFY(copy->setProperty(Prop::X, 99.0));
    QCOMPARE(object.value<double>(Prop::X), 12.5);
    QVERIFY(!copy->hasSameProperties(object));
}

// Unknown ids, wrong types and out-of-range values are rejected and leave
// the object unchanged.
inline void checkRejections(HmiObject& object)
{
    const auto before = object.clone();

    const auto unknown = object.setProperty(PropertyId{"noSuchProperty"}, 1.0);
    QVERIFY(!unknown);
    QCOMPARE(unknown.error().code, ErrorCode::UnknownProperty);

    const auto wrongType = object.setProperty(Prop::Width, QStringLiteral("wide"));
    QVERIFY(!wrongType);
    QCOMPARE(wrongType.error().code, ErrorCode::TypeMismatch);

    const auto negative = object.setProperty(Prop::Width, -1.0);
    QVERIFY(!negative);
    QCOMPARE(negative.error().code, ErrorCode::OutOfRange);

    QVERIFY(!object.setProperty(Prop::Rotation, 400.0));
    QVERIFY(object.hasSameProperties(*before));
}

// The registry creates this type by id and by key.
inline void checkRegistered(ObjectTypeId type, QStringView key)
{
    const ObjectTypeInfo* info = ObjectRegistry::instance().find(type);
    QVERIFY(info);
    QCOMPARE(info->key, key.toString());
    QCOMPARE(ObjectRegistry::instance().findByKey(key), info);
    const auto created = ObjectRegistry::instance().create(type);
    QVERIFY(created);
    QCOMPARE(created->typeId(), type);
}

// Renders the object in local coordinates onto a transparent image the size
// of the object.
[[nodiscard]] inline QImage render(const HmiObject& object, double zoom = 1.0)
{
    const QSize size = object.geometry().size().toSize().expandedTo(QSize(1, 1));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    object.paint(painter, PaintContext{.zoom = zoom});
    painter.end();
    return image;
}

[[nodiscard]] inline bool sameColour(QRgb pixel, const QColor& expected, int tolerance = 2)
{
    return std::abs(qRed(pixel) - expected.red()) <= tolerance &&
           std::abs(qGreen(pixel) - expected.green()) <= tolerance &&
           std::abs(qBlue(pixel) - expected.blue()) <= tolerance &&
           std::abs(qAlpha(pixel) - expected.alpha()) <= tolerance;
}

} // namespace hmi::test
