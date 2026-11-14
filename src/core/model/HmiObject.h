#pragma once

#include "core/Result.h"
#include "core/model/Ids.h"
#include "core/model/PaintContext.h"
#include "core/property/PropertyDescriptor.h"
#include "core/property/PropertyValue.h"

#include <QPainterPath>
#include <QRectF>
#include <QTransform>

#include <memory>
#include <optional>
#include <span>
#include <vector>

class QPainter;

namespace hmi
{

class DocumentNotifier;
class ObjectVisitor;

// Base of every object on an HMI screen. Properties are data described by
// the type's descriptor table, so the property panel, serializer and tag
// bindings work without knowing the concrete class. The object does not know
// it is drawn by Qt Quick: paint() takes a plain QPainter in local
// coordinates, from (0,0) to (width, height).
//
// While the object belongs to a Screen inside a Project, every successful
// setProperty() is announced through the document's DocumentNotifier.
class HmiObject
{
public:
    virtual ~HmiObject();
    HmiObject& operator=(const HmiObject&) = delete;
    HmiObject(HmiObject&&) = delete;
    HmiObject& operator=(HmiObject&&) = delete;

    [[nodiscard]] virtual ObjectTypeId typeId() const noexcept = 0;

    // Exact copy including the id; used for undo snapshots and comparisons.
    [[nodiscard]] virtual std::unique_ptr<HmiObject> clone() const = 0;

    // Copy with a fresh id; used for paste and duplicate.
    [[nodiscard]] std::unique_ptr<HmiObject> cloneWithNewId() const;

    // Paints in local coordinates. Runs on the render thread: reads only this
    // object's data and changes nothing.
    virtual void paint(QPainter& painter, const PaintContext& context) const = 0;

    // Area that counts as "on the object", in local coordinates. Defaults to
    // the bounding rectangle; non-rectangular types override it.
    [[nodiscard]] virtual QPainterPath hitShape() const;

    virtual void accept(ObjectVisitor& visitor) = 0;

    [[nodiscard]] std::span<const PropertyDescriptor> descriptors() const noexcept
    {
        return m_descriptors;
    }

    [[nodiscard]] std::optional<PropertyValue> property(PropertyId id) const;

    // Typed read of a property the type is known to have. Asking for a
    // property the type lacks, or with the wrong type, is a programming error.
    template <PropertyType T>
    [[nodiscard]] const T& value(PropertyId id) const
    {
        const PropertyValue* stored = find(id);
        Q_ASSERT_X(stored && std::holds_alternative<T>(*stored), "HmiObject::value",
                   "unknown property or wrong type");
        return std::get<T>(*stored);
    }

    // Validates and stores a value; notifies only if the value changed.
    Result<void> setProperty(PropertyId id, const PropertyValue& value);

    // True when both objects have the same type and property values. Ids are
    // not compared.
    [[nodiscard]] bool hasSameProperties(const HmiObject& other) const;

    [[nodiscard]] ObjectId id() const noexcept { return m_id; }
    [[nodiscard]] QString name() const;
    [[nodiscard]] QRectF geometry() const;
    [[nodiscard]] double rotation() const;
    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] bool isLocked() const;

    // Maps local coordinates to screen (scene) coordinates: translation to
    // the geometry, rotation about the centre.
    [[nodiscard]] QTransform sceneTransform() const;
    [[nodiscard]] QRectF sceneBoundingRect() const;

protected:
    // `descriptors` must outlive the object; types pass a static table.
    HmiObject(std::span<const PropertyDescriptor> descriptors, ObjectId id);

    // For clone(): copies id and values, but not the document attachment.
    HmiObject(const HmiObject& other);

private:
    friend class Screen;

    [[nodiscard]] const PropertyValue* find(PropertyId id) const noexcept;
    void setNotifier(DocumentNotifier* notifier) noexcept { m_notifier = notifier; }

    std::span<const PropertyDescriptor> m_descriptors;
    std::vector<PropertyValue> m_values;
    ObjectId m_id;
    DocumentNotifier* m_notifier = nullptr;
};

} // namespace hmi
