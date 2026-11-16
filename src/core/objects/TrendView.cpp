#include "core/objects/TrendView.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <cmath>
#include <numbers>

namespace hmi
{

namespace
{

} // namespace

TrendView::TrendView(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> TrendView::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {420.0, 220.0}, {
                            Describe::integer(Prop::TimeRange, "timeRange",
                                              QT_TRANSLATE_NOOP("hmi::Property", "Time range (s)"),
                                              Behaviour, 60, 1, 86400),
                            Describe::items(QStringLiteral("Level;Pressure")),
                            Describe::minimum(0.0),
                            Describe::maximum(100.0),
                            Describe::fillColor(QColor(Qt::white),
                                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                            Describe::color(Prop::BackgroundColor, "backgroundColor",
                                            QT_TRANSLATE_NOOP("hmi::Property", "Grid colour"),
                                            Appearance, QColor(0xE5, 0xE7, 0xEB)),
                            Describe::lineColor(QColor(0x6B, 0x72, 0x80)),
                        });
    return table;
}

std::unique_ptr<HmiObject> TrendView::clone() const
{
    return std::unique_ptr<HmiObject>(new TrendView(*this));
}

void TrendView::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    const double k = PaintHelpers::contentScale(*this);
    const QRectF plot = bounds.adjusted(8 * k, 8 * k, -8 * k, -8 * k);
    if (plot.width() <= 0 || plot.height() <= 0)
        return;
    painter.setPen(QPen(value<QColor>(Prop::BackgroundColor), 1.0));
    constexpr int kGrid = 4;
    for (int i = 1; i < kGrid; ++i)
    {
        painter.drawLine(QPointF(plot.left(), plot.top() + plot.height() * i / kGrid),
                         QPointF(plot.right(), plot.top() + plot.height() * i / kGrid));
        painter.drawLine(QPointF(plot.left() + plot.width() * i / kGrid, plot.top()),
                         QPointF(plot.left() + plot.width() * i / kGrid, plot.bottom()));
    }
    painter.setPen(QPen(value<QColor>(Prop::LineColor), 1.0));
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawLine(plot.bottomLeft(), plot.topLeft());
    // Sample curves, the same every time, to show what a trend will look
    // like; real values arrive at runtime.
    static const QColor kColors[] = {QColor(0x3B, 0x82, 0xF6), QColor(0xF5, 0x9E, 0x0B),
                                     QColor(0x22, 0xA0, 0x55), QColor(0xDC, 0x26, 0x26)};
    const QStringList traces = Describe::splitItems(value<QString>(Prop::Items));
    for (int t = 0; t < traces.size() && t < 4; ++t)
    {
        QPolygonF line;
        constexpr int kSamples = 48;
        for (int i = 0; i <= kSamples; ++i)
        {
            const double x = double(i) / kSamples;
            const double y =
                0.5 + 0.3 * std::sin((x * 2.0 + t * 0.6) * std::numbers::pi) * (1.0 - 0.3 * t);
            line.append(QPointF(plot.left() + plot.width() * x, plot.bottom() - plot.height() * y));
        }
        painter.setPen(QPen(kColors[t], 2.0 * std::min(k, 2.0)));
        painter.drawPolyline(line);
    }
}

void TrendView::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::TrendView, "trendView",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Trend view"),
                    hmi::ObjectCategory::Controls, ":/icons/trend-view.svg")
