#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Instrument bubble as in P&ID drawings: a circle with the measured
// quantity's letter (T, P, L, F) and the tag text below it.
class Sensor final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"sensor"};

    enum Kind : int
    {
        Temperature = 0,
        Pressure = 1,
        Level = 2,
        Flow = 3,
    };

    explicit Sensor(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Sensor(const Sensor&) = default;
};

} // namespace hmi
