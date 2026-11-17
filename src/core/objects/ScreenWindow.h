#pragma once

#include "core/model/ControlObject.h"

namespace hmi
{

// Shows another screen of the project inside this one, fitted into the
// window keeping its proportions. The editor draws a preview of that screen;
// in play mode the window is replaced by a live copy of it, whose values,
// buttons and inputs work as on any screen.
class ScreenWindow final : public ControlObject
{
public:
    static constexpr ObjectTypeId kTypeId{"screenWindow"};

    explicit ScreenWindow(ObjectId id = ObjectId::create());

    [[nodiscard]] static std::span<const PropertyDescriptor> staticDescriptors();

    [[nodiscard]] ObjectTypeId typeId() const noexcept override { return kTypeId; }
    [[nodiscard]] std::unique_ptr<HmiObject> clone() const override;
    void paint(QPainter& painter, const PaintContext& context) const override;
    void accept(ObjectVisitor& visitor) override;
    [[nodiscard]] std::unique_ptr<HmiObject> expandForRuntime(const IScreenSource& screens,
                                                              int depth) const override;

    // Windows inside shown screens are followed this many levels deep.
    static constexpr int kMaxDepth = 3;
    // Where a screen of `size` goes in a window of `window`: fitted and
    // centred, keeping its proportions.
    [[nodiscard]] static QRectF fitted(QSizeF size, const QRectF& window);

private:
    ScreenWindow(const ScreenWindow&) = default;
};

} // namespace hmi
