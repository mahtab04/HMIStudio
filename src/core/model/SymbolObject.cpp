#include "core/model/SymbolObject.h"

#include "core/property/Prop.h"

#include <algorithm>

namespace hmi
{

bool SymbolObject::isScaledPicture() const
{
    return findDescriptor(descriptors(), Prop::StretchMode) != nullptr;
}

QSizeF SymbolObject::designSize() const
{
    if (!isScaledPicture())
        return geometry().size();
    const auto side = [this](PropertyId id)
    {
        const PropertyDescriptor* d = findDescriptor(descriptors(), id);
        const double* value = d ? std::get_if<double>(&d->defaultValue) : nullptr;
        return value && *value > 0.0 ? *value : 1.0;
    };
    return {side(Prop::Width), side(Prop::Height)};
}

QTransform SymbolObject::designTransform() const
{
    if (!isScaledPicture())
        return {};
    const QSizeF design = designSize();
    const QSizeF size = geometry().size();
    const double sx = size.width() / design.width();
    const double sy = size.height() / design.height();
    QTransform t;
    if (value<EnumValue>(Prop::StretchMode).value == Stretch)
    {
        t.scale(sx, sy);
        return t;
    }
    // Keep proportions: as large as fits, centred.
    const double s = std::min(sx, sy);
    t.translate((size.width() - design.width() * s) / 2.0,
                (size.height() - design.height() * s) / 2.0);
    t.scale(s, s);
    return t;
}

QPainterPath SymbolObject::hitShape() const
{
    QPainterPath path;
    path.addRect(QRectF(QPointF(0, 0), designSize()));
    return designTransform().map(path);
}

} // namespace hmi
