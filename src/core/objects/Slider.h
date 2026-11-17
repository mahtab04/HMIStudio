#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// Sets a value by dragging a knob along a track at runtime, over a
// numbered scale; in the editor it shows the knob at `value`.
class Slider final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"slider"};

    explicit Slider(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;
    [[nodiscard]] RuntimeBehavior runtimeBehavior() const override;
    [[nodiscard]] std::optional<PropertyValue> runtimeValueAt(QPointF point) const override;

private:
    Slider(const Slider&) = default;
};

} // namespace hmi
