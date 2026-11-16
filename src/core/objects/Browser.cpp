#include "core/objects/Browser.h"

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

Browser::Browser(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> Browser::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {360.0, 240.0},
        {
            Describe::string(Prop::Url, "url", QT_TRANSLATE_NOOP("hmi::Property", "Address"),
                             Behaviour, QStringLiteral("https://"), true),
            Describe::flag(Prop::ShowToolbar, "showToolbar",
                           QT_TRANSLATE_NOOP("hmi::Property", "Address bar"), Appearance, true),
            Describe::fillColor(QColor(Qt::white),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
            Describe::lineColor(QColor(0xC9, 0xCE, 0xD6)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(13),
        });
    return table;
}

std::unique_ptr<HmiObject> Browser::clone() const
{
    return std::unique_ptr<HmiObject>(new Browser(*this));
}

void Browser::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    if (!value<bool>(Prop::ShowToolbar))
        return;
    // The address bar grows with the browser.
    const double k = PaintHelpers::contentScale(*this);
    const double bar = std::min(30.0 * k, size.height() / 3.0);
    painter.fillRect(QRectF(1, 1, size.width() - 2, bar), QColor(0xE5, 0xE7, 0xEB));
    const QRectF field(bar, bar / 6.0, size.width() - bar - 8 * k, bar * 2.0 / 3.0);
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(field, 4 * k, 4 * k);
    PaintHelpers::drawText(painter, *this, field.adjusted(6 * k, 0, -6 * k, 0),
                           Qt::AlignLeft | Qt::AlignVCenter, value<QString>(Prop::Url), k);
}

void Browser::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::Browser, "browser", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Browser"),
                    hmi::ObjectCategory::Controls, ":/icons/browser.svg")
