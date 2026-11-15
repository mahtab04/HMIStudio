#include "core/model/UnknownObject.h"

#include "core/model/ObjectVisitor.h"
#include "core/property/CommonDescriptors.h"

#include <QPainter>

namespace hmi
{

UnknownObject::UnknownObject(ObjectId id)
    : HmiObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> UnknownObject::staticDescriptors()
{
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors({100.0, 60.0}, {});
    return table;
}

QString UnknownObject::originalType() const
{
    return preservedJson().value(u"type").toString();
}

std::unique_ptr<HmiObject> UnknownObject::clone() const
{
    return std::unique_ptr<HmiObject>(new UnknownObject(*this));
}

void UnknownObject::paint(QPainter& painter, const PaintContext& context) const
{
    const QRectF r(QPointF(0, 0), geometry().size());
    const QColor mark(0xB0, 0x40, 0x40);
    QPen pen(mark, 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(QColor(0xB0, 0x40, 0x40, 0x18));
    painter.drawRect(r);
    painter.drawLine(r.topLeft(), r.bottomRight());
    painter.drawLine(r.bottomLeft(), r.topRight());
    if (!context.lowDetail())
    {
        painter.setPen(mark);
        painter.drawText(r, Qt::AlignCenter | Qt::TextWordWrap,
                         QStringLiteral("? %1").arg(originalType()));
    }
}

void UnknownObject::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi
