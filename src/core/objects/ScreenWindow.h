#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Shows another screen of the project inside this one at runtime
// (phase 7). In the editor it shows which screen.
class ScreenWindow final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"screenWindow"};

    explicit ScreenWindow(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    ScreenWindow(const ScreenWindow&) = default;
};

} // namespace hmi
