#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Centrifugal pump: a circle with the outlet triangle, coloured by its
// running state. Rotate the object to point the outlet another way.
class Pump final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"pump"};

    explicit Pump(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Pump(const Pump&) = default;
};

} // namespace hmi
