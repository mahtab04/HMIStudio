#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// A vessel with rounded ends, filled to `value` between minimum and maximum
// (for example a level in percent), with an optional scale.
class Tank final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"tank"};

    explicit Tank(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

    // The vessel's outline in local coordinates.
    [[nodiscard]] QPainterPath vesselPath() const;

private:
    Tank(const Tank&) = default;
};

} // namespace hmi
