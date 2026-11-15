#include "core/objects/ScreenWindow.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/PaintHelpers.h"
#include "core/property/CommonDescriptors.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QCoreApplication>
#include <QPainter>

namespace hmi
{

namespace
{

} // namespace

ScreenWindow::ScreenWindow(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> ScreenWindow::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {320.0, 200.0}, {
                            Describe::string(Prop::ScreenName, "screenName",
                                             QT_TRANSLATE_NOOP("hmi::Property", "Screen"),
                                             Behaviour, QString(), true),
                            Describe::fillColor(QColor(0xF4, 0xF5, 0xF7),
                                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
                        });
    return table;
}

std::unique_ptr<HmiObject> ScreenWindow::clone() const
{
    return std::unique_ptr<HmiObject>(new ScreenWindow(*this));
}

void ScreenWindow::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    const QString screen = value<QString>(Prop::ScreenName);
    PaintHelpers::drawPlaceholder(
        painter, bounds,
        screen.isEmpty()
            ? QCoreApplication::translate("hmi::Objects", "Screen window (no screen)")
            : QCoreApplication::translate("hmi::Objects", "Screen window: %1").arg(screen),
        value<QColor>(Prop::FillColor));
}

void ScreenWindow::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::ScreenWindow, "screenWindow",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Screen window"),
                    hmi::ObjectCategory::Controls, ":/icons/screen-window.svg")
