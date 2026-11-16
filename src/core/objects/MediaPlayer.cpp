#include "core/objects/MediaPlayer.h"

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

MediaPlayer::MediaPlayer(ObjectId id)
    : ControlObject(staticDescriptors(), id)
{
}

std::span<const PropertyDescriptor> MediaPlayer::staticDescriptors()
{
    using namespace PropertyCategory;
    static const std::vector<PropertyDescriptor> table = withCommonDescriptors(
        {320.0, 200.0},
        {
            Describe::string(Prop::Source, "source", QT_TRANSLATE_NOOP("hmi::Property", "Video"),
                             Behaviour, QString(), true),
            Describe::flag(Prop::ShowControls, "showControls",
                           QT_TRANSLATE_NOOP("hmi::Property", "Controls"), Appearance, true),
            Describe::fillColor(QColor(0x1F, 0x23, 0x28),
                                QT_TRANSLATE_NOOP("hmi::Property", "Background")),
        });
    return table;
}

std::unique_ptr<HmiObject> MediaPlayer::clone() const
{
    return std::unique_ptr<HmiObject>(new MediaPlayer(*this));
}

void MediaPlayer::paint(QPainter& painter, const PaintContext& context) const
{
    [[maybe_unused]] const QSizeF size = geometry().size();
    [[maybe_unused]] const QRectF bounds(QPointF(0, 0), size);
    Q_UNUSED(context)
    painter.fillRect(bounds, value<QColor>(Prop::FillColor));
    const double k = PaintHelpers::contentScale(*this);
    const double controls =
        value<bool>(Prop::ShowControls) ? std::min(32.0 * k, size.height() / 4.0) : 0.0;
    const QRectF picture(0, 0, size.width(), size.height() - controls);
    const double play = std::min(picture.width(), picture.height()) * 0.25;
    const QPointF c = picture.center();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.drawPolygon(QPolygonF{c + QPointF(-play * 0.4, -play / 2.0), c + QPointF(play * 0.6, 0),
                                  c + QPointF(-play * 0.4, play / 2.0)});
    if (controls > 0)
    {
        painter.fillRect(QRectF(0, picture.bottom(), size.width(), controls),
                         QColor(0x37, 0x3C, 0x44));
        painter.fillRect(QRectF(controls, picture.bottom() + controls / 2.0 - 1.5 * k,
                                size.width() - controls - 10 * k, 3 * k),
                         QColor(0x9A, 0xA1, 0xAB));
    }
}

void MediaPlayer::accept(ObjectVisitor& visitor)
{
    visitor.visit(*this);
}

} // namespace hmi

HMI_REGISTER_OBJECT(hmi::MediaPlayer, "mediaPlayer",
                    QT_TRANSLATE_NOOP("hmi::ObjectRegistry", "Media player"),
                    hmi::ObjectCategory::Controls, ":/icons/media-player.svg")
