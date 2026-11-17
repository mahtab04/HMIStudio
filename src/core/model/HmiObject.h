#pragma once

#include "core/Result.h"
#include "core/model/Events.h"
#include "core/model/Ids.h"
#include "core/model/PaintContext.h"
#include "core/model/RuntimeBehavior.h"
#include "core/property/PropertyDescriptor.h"
#include "core/property/PropertyValue.h"
#include "core/tags/Binding.h"

#include <QJsonObject>
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

    // Copy with a fresh id (and fresh ids for any children); used for paste
    // and duplicate.
    [[nodiscard]] std::unique_ptr<HmiObject> cloneWithNewId() const;

    // Objects owned by this one (Composite). Empty except for groups. Child
    // geometry is in the parent's coordinate system, not the screen's.
    [[nodiscard]] virtual std::span<const std::unique_ptr<HmiObject>> children() const noexcept
    {
        return {};
    }

    // Replaces the children of a composite type (used when reading a file or
    // the clipboard). Fails for types that cannot have children unless
    // `children` is empty.
    virtual Result<void> setChildren(std::vector<std::unique_ptr<HmiObject>> children);

    // Copies of the children converted to screen coordinates, for ungrouping.
    // Empty for types without children.
    [[nodiscard]] virtual std::vector<std::unique_ptr<HmiObject>> ungroupedChildren() const
    {
        return {};
    }

    // Paints in local coordinates. Runs on the render thread: reads only this
    // object's data and changes nothing.
    virtual void paint(QPainter& painter, const PaintContext& context) const = 0;

    // Area that counts as "on the object", in local coordinates. Defaults to
    // the bounding rectangle; non-rectangular types override it.
    [[nodiscard]] virtual QPainterPath hitShape() const;

    [[nodiscard]] virtual RuntimeBehavior runtimeBehavior() const { return {}; }
    // Value chosen at a local point (slider track or a selector row).
    [[nodiscard]] virtual std::optional<PropertyValue> runtimeValueAt(QPointF) const
    {
        return std::nullopt;
    }
    [[nodiscard]] virtual QTransform childTransform() const { return {}; }

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

    // True when both objects have the same type, property values, bindings
    // and events, and their children (if any) compare the same, in order. Ids
    // are not compared.
    [[nodiscard]] bool hasSameProperties(const HmiObject& other) const;

    // Tag bindings of this object's bindable properties, one per property.
    [[nodiscard]] std::span<const Binding> bindings() const noexcept { return m_bindings; }
    [[nodiscard]] const Binding* binding(PropertyId property) const;
    // Adds or replaces the binding of `binding.property`. Fails if the
    // property does not exist or is not bindable. Announced through the
    // document's notifier (bindingChanged).
    Result<void> setBinding(Binding binding);
    // Returns whether there was a binding to remove.
    bool removeBinding(PropertyId property);

    // What happens at runtime when the operator clicks, presses or releases
    // the object: an action list per event. Announced through the notifier
    // (eventsChanged).
    [[nodiscard]] const EventTable& events() const noexcept { return m_events; }
    void setActions(EventKind event, ActionList actions);

    // Data from a file this version does not understand (unknown
    // properties, bindings of a newer format, ...). Kept with the object and
    // written back unchanged, so opening and saving never loses it. Set and
    // read only by the serializer.
    [[nodiscard]] const QJsonObject& preservedJson() const noexcept { return m_preserved; }
    void setPreservedJson(QJsonObject json) { m_preserved = std::move(json); }

    [[nodiscard]] ObjectId id() const noexcept { return m_id; }
    [[nodiscard]] QString name() const;
    [[nodiscard]] QRectF geometry() const;
    [[nodiscard]] double rotation() const;
    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] bool isLocked() const;

    // Types with a "stretch mode" property (Describe::stretchMode) are
    // pictures, as in the symbol libraries of WinCC and other HMI editors:
    // they draw at their design size (the type's default size) and are
    // scaled to the rectangle, keeping proportions (centred) or stretched.
    // For other types the design size is the object's size and the
    // transform is the identity.
    [[nodiscard]] bool isScaledPicture() const;
    [[nodiscard]] QSizeF designSize() const;
    // Maps design coordinates to local coordinates.
    [[nodiscard]] QTransform designTransform() const;

    // Maps local coordinates to screen (scene) coordinates: translation to
    // the geometry, rotation about the centre.
    [[nodiscard]] QTransform sceneTransform() const;
    [[nodiscard]] QRectF sceneBoundingRect() const;

    // The same mapping for any geometry and rotation, for previews of a
    // geometry the object does not have yet.
    [[nodiscard]] static QTransform sceneTransform(const QRectF& geometry, double rotation);

protected:
    // `descriptors` must outlive the object; types pass a static table.
    HmiObject(std::span<const PropertyDescriptor> descriptors, ObjectId id);

    // For clone(): copies id and values, but not the document attachment.
    HmiObject(const HmiObject& other);

    // Gives this object a fresh id; types with children renew theirs too.
    virtual void renewIds();

private:
    friend class Screen;

    [[nodiscard]] const PropertyValue* find(PropertyId id) const noexcept;
    void setNotifier(DocumentNotifier* notifier) noexcept { m_notifier = notifier; }

    std::span<const PropertyDescriptor> m_descriptors;
    std::vector<PropertyValue> m_values;
    QJsonObject m_preserved;
    std::vector<Binding> m_bindings;
    EventTable m_events;
    ObjectId m_id;
    DocumentNotifier* m_notifier = nullptr;
};

} // namespace hmi
