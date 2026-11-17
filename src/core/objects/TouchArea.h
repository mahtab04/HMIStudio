#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// An invisible area that reacts to touch at runtime (events from phase 7).
// In the editor it shows a marked outline so it can be found and selected.
class TouchArea final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"touchArea"};

    explicit TouchArea(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;
    [[nodiscard]] RuntimeBehavior runtimeBehavior() const override;

private:
    TouchArea(const TouchArea&) = default;
};

} // namespace hmi
