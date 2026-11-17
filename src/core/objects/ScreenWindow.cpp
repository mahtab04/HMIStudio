#include "core/objects/ScreenWindow.h"

#include "core/model/GroupObject.h"
#include "core/model/IScreenSource.h"
#include "core/model/ObjectVisitor.h"
#include "core/model/Screen.h"
#include "core/objects/PaintHelpers.h"
#include "core/objects/Rectangle.h"
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
        {320.0, 200.0},
        {
            Describe::string(Prop::ScreenName, "screenName",
                             QT_TRANSLATE_NOOP("hmi::Property", "Screen"), Behaviour, QString()),
            Describe::fillColor(QColor(0xF4, 0xF5, 0xF7),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
        });
    return table;
}

std::unique_ptr<HmiObject> ScreenWindow::clone() const
{
    return std::unique_ptr<HmiObject>(new ScreenWindow(*this));
}

QRectF ScreenWindow::fitted(QSizeF size, const QRectF& window)
{
    if (size.isEmpty() || window.isEmpty())
        return window;
    const double scale = std::min(window.width() / size.width(), window.height() / size.height());
    const QSizeF shown = size * scale;
    return {window.left() + (window.width() - shown.width()) / 2.0,
            window.top() + (window.height() - shown.height()) / 2.0, shown.width(), shown.height()};
}

void ScreenWindow::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    const QString screen = value<QString>(Prop::ScreenName);
    // A preview of the screen it shows, when the project's screens are at
    // hand and the nesting is not too deep.
    const Screen* shown = context.screens && !screen.isEmpty() && context.depth < kMaxDepth
                              ? context.screens->screenNamed(screen)
                              : nullptr;
    if (shown)
    {
        painter.save();
        painter.fillRect(bounds, value<QColor>(Prop::FillColor));
        const QRectF area = fitted(shown->size(), bounds);
        painter.setClipRect(area, Qt::IntersectClip);
        painter.translate(area.topLeft());
        painter.scale(area.width() / shown->size().width(), area.height() / shown->size().height());
        painter.fillRect(QRectF(QPointF(), shown->size()), shown->backgroundColor());
        PaintContext nested = context;
        nested.depth = context.depth + 1;
        nested.zoom = context.zoom * area.width() / shown->size().width();
        for (const auto& object : shown->objects())
        {
            if (!object->isVisible())
                continue;
            painter.save();
            painter.setTransform(object->sceneTransform(), true);
            object->paint(painter, nested);
            painter.restore();
        }
        painter.restore();
        if (context.editing)
        {
            // Marks the window, so it is not taken for a copy of the screen.
            QPen pen(QColor(0x6B, 0x72, 0x80), 1.0, Qt::DashLine);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        return;
    }
    PaintHelpers::drawPlaceholder(
        painter, bounds,
        screen.isEmpty()
            ? QCoreApplication::translate("hmi::Objects", "Screen window (no screen)")
            : QCoreApplication::translate("hmi::Objects", "Screen window: %1").arg(screen),
        value<QColor>(Prop::FillColor), PaintHelpers::contentScale(*this));
}

std::unique_ptr<HmiObject> ScreenWindow::expandForRuntime(const IScreenSource& screens,
                                                          int depth) const
{
    const QString target = value<QString>(Prop::ScreenName);
    const Screen* shown =
        target.isEmpty() || depth >= kMaxDepth ? nullptr : screens.screenNamed(target);
    if (!shown)
        return nullptr;
    // A copy of the screen with new ids, behind it its background, the whole
    // screen size, so the group spans exactly the screen.
    auto copy = shown->cloneWithNewIds(shown->name());
    std::vector<std::unique_ptr<HmiObject>> children;
    auto background = std::make_unique<Rectangle>();
    (void)background->setProperty(Prop::X, 0.0);
    (void)background->setProperty(Prop::Y, 0.0);
    (void)background->setProperty(Prop::Width, shown->size().width());
    (void)background->setProperty(Prop::Height, shown->size().height());
    (void)background->setProperty(Prop::FillColor, shown->backgroundColor());
    (void)background->setProperty(Prop::LineWidth, 0.0);
    (void)background->setProperty(Prop::CornerRadius, 0.0);
    children.push_back(std::move(background));
    std::vector<ObjectId> ids;
    for (const auto& object : copy->objects())
        ids.push_back(object->id());
    for (const ObjectId& id : ids)
    {
        auto taken = copy->takeObject(id);
        if (!taken)
            continue;
        // Windows inside the shown screen are followed too, a level deeper.
        if (auto nested = (*taken)->expandForRuntime(screens, depth + 1))
            children.push_back(std::move(nested));
        else
            children.push_back(std::move(*taken));
    }
    // The group keeps this window's id and name; the screen fills it,
    // fitted and centred.
    auto group = GroupObject::create(std::move(children), id());
    const QRectF area = fitted(shown->size(), geometry());
    (void)group->setProperty(Prop::X, area.x());
    (void)group->setProperty(Prop::Y, area.y());
    (void)group->setProperty(Prop::Width, area.width());
    (void)group->setProperty(Prop::Height, area.height());
    (void)group->setProperty(Prop::Rotation, rotation());
    (void)group->setProperty(Prop::Name, name());
    (void)group->setProperty(Prop::Visible, isVisible());
    // Bindings of the window that the group has (visibility, position).
    for (const Binding& binding : bindings())
        (void)group->setBinding(binding);
    return group;
}

void ScreenWindow::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::ScreenWindow, "screenWindow",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Screen window"),
                    hmi::ObjectCategory::Controls, ":/icons/screen-window.svg")
