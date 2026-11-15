#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Electric motor: a ribbed housing with a shaft and an "M" badge, coloured
// by its running state.
class Motor final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"motor"};

    explicit Motor(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Motor(const Motor&) = default;
};

} // namespace hmi
