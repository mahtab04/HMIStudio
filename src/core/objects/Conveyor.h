#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Belt conveyor: a belt around two drums with idler rollers, coloured by
// its running state.
class Conveyor final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"conveyor"};

    explicit Conveyor(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Conveyor(const Conveyor&) = default;
};

} // namespace hmi
