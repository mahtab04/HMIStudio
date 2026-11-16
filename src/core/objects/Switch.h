#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// An on/off switch: a track with a knob at the left (off) or right (on)
// and the matching label.
class Switch final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"switch"};

    explicit Switch(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Switch(const Switch&) = default;
};

} // namespace hmi
