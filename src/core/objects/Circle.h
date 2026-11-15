#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Circle centred in the bounds, as large as the shorter side allows, so it
// stays round whatever the bounds (unlike Ellipse).
class Circle final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"circle"};

    explicit Circle(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

    // The circle's square in local coordinates.
    [[nodiscard]] QRectF circleRect() const;

private:
    Circle(const Circle&) = default;
};

} // namespace hmi
