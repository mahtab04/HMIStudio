#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Open line through two or more points. The points are relative to the
// bounds (0..1), so resizing scales the shape.
class Polyline final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"polyline"};

    explicit Polyline(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Polyline(const Polyline&) = default;
};

} // namespace hmi
