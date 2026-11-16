#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Plots tag values over time at runtime (phase 7). In the editor it shows
// the grid and sample curves for its traces.
class TrendView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"trendView"};

    explicit TrendView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    TrendView(const TrendView&) = default;
};

} // namespace hmi
