#pragma once

#include <QQuickItem>
#include <qqmlintegration.h>

namespace hmi
{

class DocumentNotifier;
class IGraphicProvider;
class Screen;
class ScreenItem;

// Shows one screen the way an operator panel does: fitted into the item
// keeping its aspect ratio, centred on a dark surround, with its background
// colour. No tools, rulers or selection. Used by the simulator window, and
// by the runtime later.
class RuntimeView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)

public:
    explicit RuntimeView(QQuickItem* parent = nullptr);
    ~RuntimeView() override;
    RuntimeView(const RuntimeView&) = delete;
    RuntimeView& operator=(const RuntimeView&) = delete;
    RuntimeView(RuntimeView&&) = delete;
    RuntimeView& operator=(RuntimeView&&) = delete;

    // Shows `screen` (nullptr clears). The notifier must outlive the binding.
    void setScreen(const Screen* screen, DocumentNotifier* notifier);
    void setGraphics(const IGraphicProvider* graphics);
    [[nodiscard]] ScreenItem* screenItem() const noexcept { return m_screenItem; }
    [[nodiscard]] double zoom() const noexcept { return m_zoom; }

Q_SIGNALS:
    void viewChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override;

private:
    void layout();

    ScreenItem* m_screenItem = nullptr;
    const Screen* m_screen = nullptr;
    double m_zoom = 1.0;
};

} // namespace hmi
