#pragma once

#include "core/model/HmiObject.h"

#include <QRectF>

#include <memory>
#include <vector>

namespace hmi
{

// Composite: a group owns child objects and is moved, resized and rotated as
// one object. Children keep the geometry they had when grouped, translated
// into the group's coordinates; they never change while grouped. The group
// maps the rectangle around its children onto its own geometry, so resizing
// the group scales its children and rotating it rotates them, without
// touching the children's values. That keeps undo exact.
//
// Children have no document attachment: changes reach the canvas through the
// group's own properties.
class GroupObject final : public HmiObject
{
public:
    static constexpr ObjectTypeId kTypeId{"group"};

    explicit GroupObject(ObjectId id = ObjectId::create());

    // Groups objects given in screen coordinates. The group's geometry is the
    // rectangle around the objects' rotated bounds; the objects keep their
    // order (first is drawn first) and ids.
    [[nodiscard]] static std::unique_ptr<GroupObject>
    create(std::vector<std::unique_ptr<HmiObject>> objects, ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

    [[nodiscard]] std::span<const std::unique_ptr<HmiObject>> children() const noexcept override
    {
        return m_children;
    }
    Result<void> setChildren(std::vector<std::unique_ptr<HmiObject>> children) override;

    // Maps child coordinates to the group's local coordinates.
    [[nodiscard]] QTransform childTransform() const;

    // Copies of the children in screen coordinates, with the group's
    // position, scale and rotation applied, for ungrouping. Ids are kept.
    // Exact unless a rotated child is scaled unevenly, which a rectangle
    // cannot represent; then the child keeps its own proportions as closely
    // as possible.
    [[nodiscard]] std::vector<std::unique_ptr<HmiObject>> ungroupedChildren() const;

protected:
    void renewIds() override;

private:
    GroupObject(const GroupObject& other);

    void updateChildBounds();

    std::vector<std::unique_ptr<HmiObject>> m_children;
    QRectF m_childBounds; // derived from the children; never saved
};

} // namespace hmi
