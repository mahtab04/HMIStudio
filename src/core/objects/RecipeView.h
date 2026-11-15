#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Shows and edits the data records of a recipe at runtime (phase 7). In the
// editor it shows the recipe name and the table layout.
class RecipeView final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"recipeView"};

    explicit RecipeView(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;

private:
    RecipeView(const RecipeView&) = default;
};

} // namespace hmi
