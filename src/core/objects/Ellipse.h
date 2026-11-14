#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Filled, outlined ellipse fitting the object's bounds.
class Ellipse final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"ellipse"};

    explicit Ellipse(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Ellipse(const Ellipse&) = default;
};

} // namespace hmi
