#pragma once

#include <QColor>
#include <QFont>
#include <QPointer>
#include <QQuickPaintedItem>
#include <qqmlintegration.h>

namespace hmi
{

class CanvasItem;

// Horizontal or vertical ruler showing scene coordinates of the canvas it is
// bound to. Place it alongside the canvas; it follows zoom and pan. Tick
// spacing is chosen so labels stay at least kMinimumLabelSpacing apart.
class RulerItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(hmi::CanvasItem* canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    Q_PROPERTY(
        Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(QColor background MEMBER m_background NOTIFY appearanceChanged)
    Q_PROPERTY(QColor tickColor MEMBER m_tickColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor MEMBER m_textColor NOTIFY appearanceChanged)
    Q_PROPERTY(QFont font MEMBER m_font NOTIFY appearanceChanged)

public:
    static constexpr double kMinimumLabelSpacing = 60.0;

    explicit RulerItem(QQuickItem* parent = nullptr);

    [[nodiscard]] CanvasItem* canvas() const;
    void setCanvas(CanvasItem* canvas);

    [[nodiscard]] Qt::Orientation orientation() const noexcept { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    void paint(QPainter* painter) override;

    // Scene distance between labelled ticks for a zoom: 1, 2 or 5 times a
    // power of ten, the smallest that keeps labels kMinimumLabelSpacing apart.
    [[nodiscard]] static double labelStep(double zoom);

Q_SIGNALS:
    void canvasChanged();
    void orientationChanged();
    void appearanceChanged();

private:
    QPointer<CanvasItem> m_canvas;
    QMetaObject::Connection m_viewConnection;
    Qt::Orientation m_orientation = Qt::Horizontal;
    QColor m_background{0x16, 0x19, 0x1D};
    QColor m_tickColor{0x4A, 0x50, 0x58};
    QColor m_textColor{0x8B, 0x91, 0x99};
    QFont m_font;
};

} // namespace hmi
