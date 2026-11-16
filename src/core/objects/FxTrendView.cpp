#include "core/objects/FxTrendView.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>

#include <cmath>

namespace hmi
{

namespace
{

} // namespace

FxTrendView::FxTrendView(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> FxTrendView::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {320.0, 220.0}, {
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

std::unique_ptr<HmiObject> FxTrendView::clone() const
{
    return std::unique_ptr<HmiObject>(new FxTrendView(*this));
}

void FxTrendView::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    PaintHelpers::drawFace(painter, bounds, value<QColor>(Prop::FillColor),
                           value<QColor>(Prop::LineColor), 2.0);
    const QRectF plot = bounds.adjusted(8, 8, -8, -8);
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
            const double y = std::pow(x, 1.5 + t) * 0.85 + 0.05;
            line.append(QPointF(plot.left() + plot.width() * x, plot.bottom() - plot.height() * y));
        }
        painter.setPen(QPen(kColors[t], 2.0));
        painter.drawPolyline(line);
    }
}

void FxTrendView::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::FxTrendView, "fxTrendView",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "f(x) trend view"),
                    hmi::ObjectCategory::Controls, ":/icons/fx-trend-view.svg")
