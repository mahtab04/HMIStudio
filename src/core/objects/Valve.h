#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Gate valve: two triangles point to point with an actuator stem, coloured
// by its open state.
class Valve final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"valve"};

    explicit Valve(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

    // The body (two triangles) in local coordinates.
    [[nodiscard]] QPainterPath bodyPath() const;

private:
    Valve(const Valve&) = default;
};

} // namespace hmi
