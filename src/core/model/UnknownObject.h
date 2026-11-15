#pragma once

#include "core/model/HmiObject.h"

namespace hmi
{

// Stands in for an object whose type this version does not know (a file
// from a newer version, or a type from a plug-in that is not installed).
// It has the common properties, so it can be selected, moved and deleted,
// and draws a marked placeholder. The original entry is kept in
// preservedJson() and written back with only the common properties
// updated, so nothing of the unknown object is lost.
//
// Not in the ObjectRegistry: ObjectJson creates it when a type is unknown.
class UnknownObject final : public HmiObject
{
public:
    static constexpr ObjectTypeId kTypeId{"unknown"};

    explicit UnknownObject(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    // The type key from the file.
    [[nodiscard]] QString originalType() const;

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    UnknownObject(const UnknownObject&) = default;
};

} // namespace hmi
