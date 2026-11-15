#include "core/model/GroupObject.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hmi
{

namespace
{

// Ungrouping goes through rotations and scales; values this close to a
// round number are float noise and are cleaned up.
double clean(double value)
{
    constexpr double kPrecision = 1e6;
    return std::round(value * kPrecision) / kPrecision;
}

QRectF boundsOf(std::span<const std::unique_ptr<HmiObject>> objects)
{
    if (objects.empty())
        return {};
    double left = std::numeric_limits<double>::max();
    double top = left;
    double right = std::numeric_limits<double>::lowest();
    double bottom = right;
    for (const auto& object : objects)
    {
        const QRectF r = object->sceneBoundingRect();
        left = std::min(left, r.left());
        top = std::min(top, r.top());
        right = std::max(right, r.right());
        bottom = std::max(bottom, r.bottom());
    }
    return {QPointF(left, top), QPointF(right, bottom)};
}

bool setGeometry(HmiObject& object, const QRectF& g, double rotation)
{
    return object.setProperty(Prop::X, g.x()) && object.setProperty(Prop::Y, g.y()) &&
           object.setProperty(Prop::Width, g.width()) &&
           object.setProperty(Prop::Height, g.height()) &&
           object.setProperty(Prop::Rotation, rotation);
}

} // namespace

GroupObject::GroupObject(ObjectId id)
    : HmiObject(staticDescriptors(), id)
{
}

GroupObject::GroupObject(const GroupObject& other)
    : HmiObject(other)
    , m_childBounds(other.m_childBounds)
{
    m_children.reserve(other.m_children.size());
    for (const auto& child : other.m_children)
        m_children.push_back(child->clone());
}

std::unique_ptr<GroupObject> GroupObject::create(std::vector<std::unique_ptr<HmiObject>> objects,
                                                 ObjectId id)
{
    auto group = std::make_unique<GroupObject>(id);
    const QRectF bounds = boundsOf(objects);
    for (const auto& object : objects)
    {
        const QRectF g = object->geometry().translated(-bounds.topLeft());
        [[maybe_unused]] const bool moved =
            object->setProperty(Prop::X, g.x()) && object->setProperty(Prop::Y, g.y());
        Q_ASSERT(moved);
    }
    [[maybe_unused]] const bool placed = setGeometry(*group, bounds, 0.0);
    Q_ASSERT(placed);
    [[maybe_unused]] const auto adopted = group->setChildren(std::move(objects));
    Q_ASSERT(adopted);
    return group;
}

std::span<const PropertyDescriptor> GroupObject::staticDescriptors()
{
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors({100.0, 100.0}, {});
    return table;
}

std::unique_ptr<HmiObject> GroupObject::clone() const
{
    return std::unique_ptr<HmiObject>(new GroupObject(*this));
}

Result<void> GroupObject::setChildren(std::vector<std::unique_ptr<HmiObject>> children)
{
    if (std::ranges::any_of(children, [](const auto& child) { return !child; }))
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("A group child is null"));
    m_children = std::move(children);
    updateChildBounds();
    return {};
}

void GroupObject::updateChildBounds()
{
    m_childBounds = boundsOf(m_children);
}

QTransform GroupObject::childTransform() const
{
    const QSizeF size = geometry().size();
    const double sx = m_childBounds.width() > 0.0 ? size.width() / m_childBounds.width() : 1.0;
    const double sy = m_childBounds.height() > 0.0 ? size.height() / m_childBounds.height() : 1.0;
    QTransform t;
    t.scale(sx, sy);
    t.translate(-m_childBounds.x(), -m_childBounds.y());
    return t;
}

void GroupObject::paint(QPainter& painter, const PaintContext& context) const
{
    painter.save();
    painter.setTransform(childTransform(), true);
    for (const auto& child : m_children)
    {
        if (!child->isVisible())
            continue;
        painter.save();
        painter.setTransform(child->sceneTransform(), true);
        child->paint(painter, context);
        painter.restore();
    }
    painter.restore();
}

QPainterPath GroupObject::hitShape() const
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const auto& child : m_children)
        path.addPath(child->sceneTransform().map(child->hitShape()));
    return childTransform().map(path);
}

std::vector<std::unique_ptr<HmiObject>> GroupObject::ungroupedChildren() const
{
    const QTransform toScene = childTransform() * sceneTransform();
    std::vector<std::unique_ptr<HmiObject>> result;
    result.reserve(m_children.size());
    for (const auto& child : m_children)
    {
        // Follow the child's own axes through the full mapping: their scene
        // lengths give the new size, the x axis the new rotation.
        const QSizeF size = child->geometry().size();
        const QTransform full = child->sceneTransform() * toScene;
        const QPointF origin = full.map(QPointF(0, 0));
        const QPointF xAxis = full.map(QPointF(1, 0)) - origin;
        const QPointF yAxis = full.map(QPointF(0, 1)) - origin;
        const QPointF centre = full.map(QPointF(size.width() / 2.0, size.height() / 2.0));
        const QSizeF newSize(clean(size.width() * std::hypot(xAxis.x(), xAxis.y())),
                             clean(size.height() * std::hypot(yAxis.x(), yAxis.y())));
        double rotation = clean(qRadiansToDegrees(std::atan2(xAxis.y(), xAxis.x())));
        if (rotation <= -180.0)
            rotation += 360.0;

        auto copy = child->clone();
        const QRectF g(QPointF(clean(centre.x() - newSize.width() / 2.0),
                               clean(centre.y() - newSize.height() / 2.0)),
                       newSize);
        [[maybe_unused]] const bool placed = setGeometry(*copy, g, rotation);
        Q_ASSERT(placed);
        result.push_back(std::move(copy));
    }
    return result;
}

void GroupObject::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

void GroupObject::renewIds()
{
    HmiObject::renewIds();
    for (auto& child : m_children)
        child = child->cloneWithNewId();
}

} // namespace hmi

HMI_REGISTER_INTERNAL_OBJECT(hmi::GroupObject, "group",
                             QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Group"))
