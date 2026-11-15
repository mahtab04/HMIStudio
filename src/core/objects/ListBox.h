#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// A list of entries with one highlighted (`selectedIndex`).
class ListBox final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"listBox"};

    explicit ListBox(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    ListBox(const ListBox&) = default;
};

} // namespace hmi
