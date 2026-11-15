#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A check box with a caption.
class CheckBox final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"checkBox"};

    explicit CheckBox(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    CheckBox(const CheckBox&) = default;
};

} // namespace hmi
