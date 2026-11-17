#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A framed set of mutually exclusive options, one per line, with shaded
// round buttons; `selectedIndex` is the chosen one.
class RadioGroup final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"radioGroup"};

    explicit RadioGroup(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;
    [[nodiscard]] RuntimeBehavior runtimeBehavior() const override;
    [[nodiscard]] std::optional<PropertyValue> runtimeValueAt(QPointF point) const override;

private:
    RadioGroup(const RadioGroup&) = default;
};

} // namespace hmi
