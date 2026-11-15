#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Shows a web page or document at runtime (phase 7). In the editor it
// shows the address bar and the address.
class Browser final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"browser"};

    explicit Browser(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    Browser(const Browser&) = default;
};

} // namespace hmi
