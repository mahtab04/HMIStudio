#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A push button with a caption. What it does when pressed is set with
// events in phase 7.
class Button final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"button"};

    explicit Button(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Button(const Button&) = default;
};

} // namespace hmi
