#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// Shows a number and, in input mode, lets the operator enter one at runtime.
// The value is formatted with a fixed number of decimals and a unit.
class IoField final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"ioField"};

    enum Mode : int
    {
        Output = 0,
        Input = 1,
        InputOutput = 2,
    };

    explicit IoField(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

    // The value as shown: fixed decimals, then the unit.
    [[nodiscard]] QString displayText() const;

private:
    IoField(const IoField&) = default;
};

} // namespace hmi
