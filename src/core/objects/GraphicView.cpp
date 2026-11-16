#include "core/objects/GraphicView.h"

#include "core/graphics/IGraphicProvider.h"
#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QPainter>

#include <array>

namespace hmi
{

namespace
{

constexpr std::array kFitOptions{
    EnumOption{.value = int(GraphicFit::Stretch),
               .key = "stretch",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Stretch")},
    EnumOption{.value = int(GraphicFit::Fit),
               .key = "fit",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Fit, keep proportions")},
    EnumOption{.value = int(GraphicFit::Fill),
               .key = "fill",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Fill, keep proportions")},
    EnumOption{.value = int(GraphicFit::Centre),
               .key = "centre",
               .displayName = QT_TRANSLATE_NOOP("hmi::Property", "Natural size, centred")},
};

} // namespace

GraphicView::GraphicView(ObjectId id)
    : ShapeObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> GraphicView::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {120.0, 90.0},
        {
            Describe::string(Prop::Asset, "asset", QT_TRANSLATE_NOOP("hmi::Property", "Graphic"),
                             Appearance, QString()),
            Describe::choice(Prop::FitMode, "fitMode", QT_TRANSLATE_NOOP("hmi::Property", "Fit"),
                             Appearance, int(GraphicFit::Fit), kFitOptions),
            Describe::string(Prop::StyleOverrides, "styleOverrides",
                             QT_TRANSLATE_NOOP("hmi::Property", "Style overrides"), Appearance,
                             QString(), true),
            Describe::fillColor(QColor(Qt::transparent),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
        });
    return table;
}

std::unique_ptr<HmiObject> GraphicView::clone() const
{
    return std::unique_ptr<HmiObject>(new GraphicView(*this));
}

void GraphicView::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    painter.fillRect(bounds, value<QColor>(Prop::FillColor));
    const QString asset = value<QString>(Prop::Asset);
    // A style that does not parse is ignored rather than hiding the graphic.
    const GraphicStyle style =
        GraphicStyle::parse(value<QString>(Prop::StyleOverrides)).value_or(GraphicStyle{});
    const auto fit = static_cast<GraphicFit>(value<EnumValue>(Prop::FitMode).value);
    if (!asset.isEmpty() && context.graphics &&
        context.graphics->draw(painter, asset, bounds, fit, style))
        return;
    // No graphic yet, or it is missing: a marked frame, so the object can
    // be found and given one.
    PaintHelpers::drawPlaceholder(
        painter, bounds,
        asset.isEmpty() ? QCoreApplication::translate("hmi::Objects", "No graphic")
                        : QCoreApplication::translate("hmi::Objects", "Missing graphic"),
        QColor(0xF4, 0xF5, 0xF7));
}

void GraphicView::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::GraphicView, "graphicView",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Graphic view"),
                    hmi::ObjectCategory::BasicObjects, ":/icons/graphic-view.svg")
