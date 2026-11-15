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
    QPainterPath path;
    path.addRect(QRectF(QPointF(0, 0), geometry().size()));
    return path;
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
    if (typeId() != other.typeId() || m_values != other.m_values)
        return false;
    const auto mine = children();
    const auto theirs = other.children();
    return std::ranges::equal(mine, theirs, [](const auto& a, const auto& b)
                              { return a->hasSameProperties(*b); });
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
