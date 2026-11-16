#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A bar graph in a dark body: the part of the track between minimum and
// value is filled, vertically from the bottom or horizontally from the
// left, beside a numbered scale.
class Bar final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"bar"};

    explicit Bar(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Bar(const Bar&) = default;
};

} // namespace hmi
