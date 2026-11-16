#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Plots one tag against another (y over x) at runtime (phase 7), for
// characteristic curves. In the editor it shows the grid and sample curves.
class FxTrendView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"fxTrendView"};

    explicit FxTrendView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    FxTrendView(const FxTrendView&) = default;
};

} // namespace hmi
