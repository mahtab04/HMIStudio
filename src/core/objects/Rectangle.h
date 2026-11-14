#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Filled, outlined rectangle with optional rounded corners.
class Rectangle final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"rectangle"};

    explicit Rectangle(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Rectangle(const Rectangle&) = default;
};

} // namespace hmi
