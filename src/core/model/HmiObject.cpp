#include "core/model/HmiObject.h"

#include "core/model/DocumentNotifier.h"
#include "core/property/Prop.h"

#include <algorithm>

namespace hmi
{

HmiObject::HmiObject(std::span<const PropertyDescriptor> descriptors, ObjectId id)
    : m_descriptors(descriptors)
    , m_id(id)
{
    Q_ASSERT(!m_id.isNull());
    m_values.reserve(m_descriptors.size());
    for (const PropertyDescriptor& d : m_descriptors)
        m_values.push_back(d.defaultValue);
}

HmiObject::HmiObject(const HmiObject& other)
    : m_descriptors(other.m_descriptors)
    , m_values(other.m_values)
    , m_preserved(other.m_preserved)
    , m_bindings(other.m_bindings)
    , m_id(other.m_id)
{
}

HmiObject::~HmiObject() = default;

std::unique_ptr<HmiObject> HmiObject::cloneWithNewId() const
{
    std::unique_ptr<HmiObject> copy = clone();
    copy->renewIds();
    return copy;
}

void HmiObject::renewIds()
{
    m_id = ObjectId::create();
}

QPainterPath HmiObject::hitShape() const
{
    // The design rectangle, mapped: a picture kept in proportion does not
    // cover the empty margins of its rectangle.
    QPainterPath path;
    path.addRect(QRectF(QPointF(0, 0), designSize()));
    return designTransform().map(path);
}

bool HmiObject::isScaledPicture() const
{
    return find(Prop::StretchMode) != nullptr;
}

QSizeF HmiObject::designSize() const
{
    if (!isScaledPicture())
        return geometry().size();
    const auto side = [this](PropertyId id)
    {
        const PropertyDescriptor* d = findDescriptor(m_descriptors, id);
        const double* value = d ? std::get_if<double>(&d->defaultValue) : nullptr;
        return value && *value > 0.0 ? *value : 1.0;
    };
    return {side(Prop::Width), side(Prop::Height)};
}

QTransform HmiObject::designTransform() const
{
    if (!isScaledPicture())
        return {};
    const QSizeF design = designSize();
    const QSizeF size = geometry().size();
    const double sx = size.width() / design.width();
    const double sy = size.height() / design.height();
    QTransform t;
    if (value<EnumValue>(Prop::StretchMode).value == 1) // stretch
    {
        t.scale(sx, sy);
        return t;
    }
    const double s = std::min(sx, sy);
    t.translate((size.width() - design.width() * s) / 2.0,
                (size.height() - design.height() * s) / 2.0);
    t.scale(s, s);
    return t;
}

const PropertyValue* HmiObject::find(PropertyId id) const noexcept
{
    const auto it = std::ranges::find(m_descriptors, id, &PropertyDescriptor::id);
    if (it == m_descriptors.end())
        return nullptr;
    return &m_values[std::size_t(it - m_descriptors.begin())];
}

std::optional<PropertyValue> HmiObject::property(PropertyId id) const
{
    if (const PropertyValue* stored = find(id))
        return *stored;
    return std::nullopt;
}

Result<void> HmiObject::setProperty(PropertyId id, const PropertyValue& value)
{
    const auto it = std::ranges::find(m_descriptors, id, &PropertyDescriptor::id);
    if (it == m_descriptors.end())
    {
        return makeError(ErrorCode::UnknownProperty,
                         QStringLiteral("Object type has no property with id %1").arg(id.value()));
    }

    Result<PropertyValue> checked = validate(*it, value);
    if (!checked)
        return tl::unexpected(checked.error());

    PropertyValue& stored = m_values[std::size_t(it - m_descriptors.begin())];
    if (stored == *checked)
        return {};

    stored = std::move(*checked);
    if (m_notifier)
        Q_EMIT m_notifier->propertyChanged(m_id, id);
    return {};
}

bool HmiObject::hasSameProperties(const HmiObject& other) const
{
    if (typeId() != other.typeId() || m_values != other.m_values || m_bindings != other.m_bindings)
        return false;
    const auto mine = children();
    const auto theirs = other.children();
    return std::ranges::equal(mine, theirs, [](const auto& a, const auto& b)
                              { return a->hasSameProperties(*b); });
}

const Binding* HmiObject::binding(PropertyId property) const
{
    const auto it = std::ranges::find(m_bindings, property, &Binding::property);
    return it == m_bindings.end() ? nullptr : &*it;
}

Result<void> HmiObject::setBinding(Binding binding)
{
    const PropertyDescriptor* d = findDescriptor(m_descriptors, binding.property);
    if (!d || !d->bindable)
        return makeError(
            ErrorCode::UnknownProperty,
            QStringLiteral("Property %1 cannot be bound to a tag").arg(binding.property.value()));
    if (!Tag::isValidName(binding.tag))
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("A binding needs a tag"));
    if (!binding.transform)
        binding.transform = Binding::direct();
    const PropertyId property = binding.property;
    if (auto it = std::ranges::find(m_bindings, property, &Binding::property);
        it != m_bindings.end())
    {
        if (*it == binding)
            return {};
        *it = std::move(binding);
    }
    else
    {
        m_bindings.push_back(std::move(binding));
    }
    if (m_notifier)
        Q_EMIT m_notifier->bindingChanged(m_id, property);
    return {};
}

bool HmiObject::removeBinding(PropertyId property)
{
    if (std::erase_if(m_bindings,
                      [property](const Binding& b) { return b.property == property; }) == 0)
        return false;
    if (m_notifier)
        Q_EMIT m_notifier->bindingChanged(m_id, property);
    return true;
}

Result<void> HmiObject::setChildren(std::vector<std::unique_ptr<HmiObject>> children)
{
    if (children.empty())
        return {};
    return makeError(ErrorCode::InvalidArgument,
                     QStringLiteral("Object type %1 cannot have children").arg(typeId().value()));
}

QString HmiObject::name() const
{
    return value<QString>(Prop::Name);
}

QRectF HmiObject::geometry() const
{
    return {value<double>(Prop::X), value<double>(Prop::Y), value<double>(Prop::Width),
            value<double>(Prop::Height)};
}

double HmiObject::rotation() const
{
    return value<double>(Prop::Rotation);
}

bool HmiObject::isVisible() const
{
    return value<bool>(Prop::Visible);
}

bool HmiObject::isLocked() const
{
    return value<bool>(Prop::Locked);
}

QTransform HmiObject::sceneTransform() const
{
    return sceneTransform(geometry(), rotation());
}

QTransform HmiObject::sceneTransform(const QRectF& geometry, double rotation)
{
    QTransform t;
    t.translate(geometry.x() + geometry.width() / 2.0, geometry.y() + geometry.height() / 2.0);
    t.rotate(rotation);
    t.translate(-geometry.width() / 2.0, -geometry.height() / 2.0);
    return t;
}

QRectF HmiObject::sceneBoundingRect() const
{
    return sceneTransform().mapRect(QRectF(QPointF(0, 0), geometry().size()));
}

} // namespace hmi
