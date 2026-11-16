#pragma once

#include "core/model/SymbolObject.h"

namespace hmi
{

// Heat exchanger: a circle crossed by a zigzag tube.
class Exchanger final : public SymbolObject
{
public:
    static constexpr ObjectTypeId kTypeId{"exchanger"};

    explicit Exchanger(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    [[nodiscard]] QPainterPath hitShape() const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Exchanger(const Exchanger&) = default;
};

} // namespace hmi
