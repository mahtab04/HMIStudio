#pragma once

#include "core/model/Ids.h"
#include "core/model/Screen.h"
#include "core/property/PropertyValue.h"

#include <QColor>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <memory>

namespace hmi
{

class HmiObject;

// Fluent assembly of a Screen for templates and test fixtures:
//
//     auto screen = ScreenBuilder(u"Overview"_s)
//                       .size({1280, 800})
//                       .add(Rectangle::kTypeId, {10, 10, 100, 60})
//                       .with(Prop::FillColor, QColor(Qt::red))
//                       .add(Ellipse::kTypeId, {200, 10, 80, 80})
//                       .build();
//
// Objects come from the ObjectRegistry. Invalid steps (unknown type, a
// rejected property value) are programming errors in a fixture and assert.
class ScreenBuilder
{
public:
    explicit ScreenBuilder(QString name);
    ~ScreenBuilder();
    ScreenBuilder(const ScreenBuilder&) = delete;
    ScreenBuilder& operator=(const ScreenBuilder&) = delete;
    ScreenBuilder(ScreenBuilder&&) noexcept;
    ScreenBuilder& operator=(ScreenBuilder&&) noexcept;

    ScreenBuilder& size(QSizeF size);
    ScreenBuilder& background(const QColor& color);

    // Adds an object of the registered type with the given geometry, on top.
    ScreenBuilder& add(ObjectTypeId type, const QRectF& geometry);

    // Sets a property on the most recently added object.
    ScreenBuilder& with(PropertyId id, const PropertyValue& value);

    // Id of the most recently added object, to refer to it in assertions.
    [[nodiscard]] ObjectId lastId() const;

    [[nodiscard]] std::unique_ptr<Screen> build();

private:
    std::unique_ptr<Screen> m_screen;
    HmiObject* m_last = nullptr;
};

} // namespace hmi
