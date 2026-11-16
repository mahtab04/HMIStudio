#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Shows the state of connections and devices at runtime (phase 8). In the
// editor it shows the table layout.
class DiagnosticsView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"diagnosticsView"};

    explicit DiagnosticsView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    DiagnosticsView(const DiagnosticsView&) = default;
};

} // namespace hmi
