#include "core/objects/Button.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <algorithm>

namespace hmi
{

namespace
{

} // namespace

Button::Button(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Button::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 40.0},
        {
            Describe::tagAction(),
            Describe::actionTag(),
            Describe::actionValue(),
            Describe::text(QStringLiteral("Button")),
            Describe::fillColor(QColor(0xC9, 0xCE, 0xD6)),
            Describe::styled(Describe::lineColor(QColor(0x7C, 0x85, 0x90)), StyleRole::Line),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(14),
        });
    return table;
}

std::unique_ptr<HmiObject> Button::clone() const
{
    return std::unique_ptr<HmiObject>(new Button(*this));
}

void Button::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawRaisedFace(painter, bounds, value<QColor>(Prop::FillColor),
                                 value<QColor>(Prop::LineColor),
                                 std::min(4.0, size.height() / 4.0));
    // The label grows with the button.
    const double k = PaintHelpers::contentScale(*this);
    PaintHelpers::drawText(painter, *this, bounds.adjusted(4 * k, 0, -4 * k, 0),
                           Qt::AlignCenter | Qt::TextWordWrap, value<QString>(Prop::Text), k);
}

RuntimeBehavior Button::runtimeBehavior() const
{
    return {RuntimeBehavior::Kind::Action, {}};
}

void Button::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Button, "button", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Button"),
                    hmi::ObjectCategory::Elements, ":/icons/button.svg")
