#pragma once

#include "core/model/ShapeObject.h"

#include <QLineF>

namespace hmi
{

// Straight line across the object's bounds. The bounds may have zero width
// or height (horizontal or vertical line); `lineDirection` chooses which
// diagonal the line follows.
class Line final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"line"};

    enum Direction : int
    {
        Falling = 0, // top-left to bottom-right
        Rising = 1,  // bottom-left to top-right
    };

    explicit Line(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

    // The line in local coordinates.
    [[nodiscard]] QLineF localLine() const;

private:
    Line(const Line&) = default;
};

} // namespace hmi
