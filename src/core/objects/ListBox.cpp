#include "core/objects/ListBox.h"

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

ListBox::ListBox(ObjectId id)
    : ElementObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> ListBox::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {140.0, 110.0}, {
                            Describe::items(QStringLiteral("Item 1;Item 2;Item 3;Item 4")),
                            Describe::integer(Prop::SelectedIndex, "selectedIndex",
                                              QT_TRANSLATE_NOOP("hmi::Property", "Selected"),
                                              Behaviour, 0, -1, std::nullopt, true),
                            Describe::fillColor(QColor(Qt::white),
                                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                            Describe::lineColor(QColor(0x9A, 0xA1, 0xAB)),
                            Describe::color(Prop::AccentColor, "accentColor",
                                            QT_TRANSLATE_NOOP("hmi::Property", "Highlight colour"),
                                            Appearance, QColor(0xDB, 0xE8, 0xFE)),
                            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
                            Describe::font(14),
                        });
    return table;
}

std::unique_ptr<HmiObject> ListBox::clone() const
{
    return std::unique_ptr<HmiObject>(new ListBox(*this));
}

void ListBox::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    const QStringList items = Describe::splitItems(value<QString>(Prop::Items));
    const double rowHeight = std::max(18.0, value<QFont>(Prop::Font).pixelSize() * 1.6);
    painter.save();
    painter.setClipRect(bounds.adjusted(1, 1, -1, -1));
    for (int i = 0; i < items.size() && i * rowHeight < size.height(); ++i)
    {
        const QRectF row(1.0, 1.0 + i * rowHeight, size.width() - 2.0, rowHeight);
        if (i == value<int>(Prop::SelectedIndex))
            painter.fillRect(row, value<QColor>(Prop::AccentColor));
        PaintHelpers::drawText(painter, *this, row.adjusted(6, 0, -6, 0),
                               Qt::AlignLeft | Qt::AlignVCenter, items[i]);
    }
    painter.restore();
}

void ListBox::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::ListBox, "listBox", QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "List box"),
                    hmi::ObjectCategory::Elements, ":/icons/list-box.svg")
