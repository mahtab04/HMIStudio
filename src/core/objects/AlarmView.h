#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Lists active and past alarms at runtime (phase 7). In the editor it shows
// its layout: toolbar, column headers and empty rows.
class AlarmView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"alarmView"};

    explicit AlarmView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    AlarmView(const AlarmView&) = default;
};

} // namespace hmi
