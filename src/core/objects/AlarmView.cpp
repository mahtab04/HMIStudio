#include "core/objects/AlarmView.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QPainter>

#include <algorithm>

namespace hmi
{

namespace
{

} // namespace

AlarmView::AlarmView(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> AlarmView::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {420.0, 220.0},
        {
            Describe::string(Prop::Columns, "columns",
                             QT_TRANSLATE_NOOP("hmi::Property", "Columns (a;b;c)"), Layout,
                             QStringLiteral("Time;Class;Message;State")),
            Describe::integer(Prop::RowCount, "rowCount",
                              QT_TRANSLATE_NOOP("hmi::Property", "Rows"), Layout, 8, 1, 100),
            Describe::flag(Prop::ShowToolbar, "showToolbar",
                           QT_TRANSLATE_NOOP("hmi::Property", "Toolbar"), Appearance, true),
            Describe::fillColor(QColor(Qt::white),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
            Describe::color(Prop::BackgroundColor, "backgroundColor",
                            QT_TRANSLATE_NOOP("hmi::Property", "Header colour"), Appearance,
                            QColor(0xE5, 0xE7, 0xEB)),
            Describe::lineColor(QColor(0xC9, 0xCE, 0xD6)),
            Describe::textColor(QColor(0x1F, 0x23, 0x28)),
            Describe::font(13),
        });
    return table;
}

std::unique_ptr<HmiObject> AlarmView::clone() const
{
    return std::unique_ptr<HmiObject>(new AlarmView(*this));
}

void AlarmView::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    // Rows and text grow with the view.
    const double k = PaintHelpers::contentScale(*this);
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    double top = 1.0;
    const double rowHeight = std::max(16.0, value<QFont>(Prop::Font).pixelSize() * 1.7) * k;
    if (value<bool>(Prop::ShowToolbar))
    {
        painter.fillRect(QRectF(1, top, size.width() - 2, rowHeight),
                         value<QColor>(Prop::BackgroundColor));
        PaintHelpers::drawText(painter, *this, QRectF(6 * k, top, size.width() - 12 * k, rowHeight),
                               Qt::AlignLeft | Qt::AlignVCenter,
                               QCoreApplication::translate("hmi::Objects", "Alarms"), k);
        top += rowHeight;
    }
    const QStringList columns = Describe::splitItems(value<QString>(Prop::Columns));
    painter.fillRect(QRectF(1, top, size.width() - 2, rowHeight),
                     value<QColor>(Prop::BackgroundColor).darker(104));
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    const double columnWidth = columns.isEmpty() ? size.width() : size.width() / columns.size();
    for (int i = 0; i < columns.size(); ++i)
    {
        const QRectF cell(i * columnWidth, top, columnWidth, rowHeight);
        if (i > 0)
            painter.drawLine(cell.topLeft(), QPointF(cell.left(), size.height() - 1));
        PaintHelpers::drawText(painter, *this, cell.adjusted(6 * k, 0, -4 * k, 0),
                               Qt::AlignLeft | Qt::AlignVCenter, columns[i], k);
    }
    top += rowHeight;
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    for (int r = 0; r < value<int>(Prop::RowCount) && top + rowHeight * (r + 1) < size.height();
         ++r)
    {
        const double y = top + rowHeight * (r + 1);
        painter.drawLine(QPointF(1, y), QPointF(size.width() - 1, y));
    }
}

void AlarmView::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::AlarmView, "alarmView",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Alarm view"),
                    hmi::ObjectCategory::Controls, ":/icons/alarm-view.svg")
