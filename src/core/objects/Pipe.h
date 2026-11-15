#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// A pipe run through two or more points (relative to the bounds, routed
// with right angles by the pipe tool). `lineWidth` is the pipe's diameter;
// while `flowing`, a dashed line shows the flow.
class Pipe final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"pipe"};

    explicit Pipe(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Pipe(const Pipe&) = default;
};

} // namespace hmi
