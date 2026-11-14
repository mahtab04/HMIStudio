#pragma once

#include "core/model/ShapeObject.h"

namespace hmi
{

// Static text with font, colour, alignment and an optional background.
class TextBox final : public ShapeObject
{
public:
    static constexpr ObjectTypeId kTypeId{"textBox"};

    enum HorizontalAlignment : int
    {
        AlignLeft = 0,
        AlignHCenter = 1,
        AlignRight = 2,
    };

    enum VerticalAlignment : int
    {
        AlignTop = 0,
        AlignVCenter = 1,
        AlignBottom = 2,
    };

    explicit TextBox(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

    // Qt alignment and wrap flags for QPainter::drawText.
    [[nodiscard]] int textFlags() const;

private:
    TextBox(const TextBox&) = default;
};

} // namespace hmi
