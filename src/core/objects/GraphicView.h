#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Shows an imported image or SVG graphic (ARCHITECTURE.md section 14). The
// object stores only the asset id, how the graphic fits its rectangle, and
// style overrides for named SVG parts; PaintContext::graphics draws it.
class GraphicView final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"graphicView"};

    explicit GraphicView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    GraphicView(const GraphicView&) = default;
};

} // namespace hmi
