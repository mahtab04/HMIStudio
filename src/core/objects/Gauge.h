#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A round dial: a 270 degree scale band with numbered ticks from minimum to
// maximum, a value arc and needle at `value`, and the value as digits in a
// dark centre. Scaled as a picture.
class Gauge final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"gauge"};

    explicit Gauge(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Gauge(const Gauge&) = default;
};

} // namespace hmi
