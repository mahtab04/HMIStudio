#include "core/objects/TouchArea.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

namespace hmi
{

namespace
{

} // namespace

TouchArea::TouchArea(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> TouchArea::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors({120.0, 80.0}, {

                                                                                              });
    return table;
}

std::unique_ptr<HmiObject> TouchArea::clone() const
{
    return std::unique_ptr<HmiObject>(new TouchArea(*this));
}

void TouchArea::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    if (!context.editing)
        return;
    painter.save();
    painter.setBrush(QBrush(QColor(0x3B, 0x82, 0xF6, 0x40), Qt::BDiagPattern));
    QPen pen(QColor(0x3B, 0x82, 0xF6), 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
}

void TouchArea::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::TouchArea, "touchArea",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Touch area"),
                    hmi::ObjectCategory::Elements, ":/icons/touch-area.svg")
