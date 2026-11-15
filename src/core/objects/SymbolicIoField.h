#pragma once

#include "core/model/ElementObject.h"

namespace hmi
{

// Shows a text chosen by a number (0 = first entry, ...), for states such
// as Stopped / Running / Fault; in input mode it offers the texts as a
// drop-down at runtime.
class SymbolicIoField final : public ElementObject
{
public:
    static constexpr ObjectTypeId kTypeId{"symbolicIoField"};

    explicit SymbolicIoField(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    SymbolicIoField(const SymbolicIoField&) = default;
};

} // namespace hmi
