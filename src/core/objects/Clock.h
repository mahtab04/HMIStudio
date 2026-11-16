#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// Shows the time: an analogue dark face with numbers, minute ticks and
// light hands, or digits. The editor always shows 10:10:30, so screens
// look the same every time they are drawn; the runtime shows the current
// time. Scaled as a picture.
class Clock final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"clock"};

    enum Mode : int
    {
        Analog = 0,
        Digital = 1,
    };

    explicit Clock(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Clock(const Clock&) = default;
};

} // namespace hmi
