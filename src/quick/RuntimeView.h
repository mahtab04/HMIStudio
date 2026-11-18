#pragma once

#include "quick/input/GestureRecognizer.h"

#include <QQuickItem>
#include <qqmlintegration.h>

namespace hmi
{

class DocumentNotifier;
class IAlarmSource;
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
    // The running alarms, for alarm controls.
    void setAlarms(const IAlarmSource* alarms);
    [[nodiscard]] ScreenItem* screenItem() const noexcept { return m_screenItem; }
    [[nodiscard]] double zoom() const noexcept { return m_zoom; }

Q_SIGNALS:
    void viewChanged();
    void gesture(const hmi::PointerGesture& gesture);
    void keyInput(int key, Qt::KeyboardModifiers modifiers, bool released);
    void inputCancelled();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent* event) override;
    void touchUngrabEvent() override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void layout();

    InputSettings m_settings;
    GestureRecognizer m_recognizer;
    QMetaObject::Connection m_activationConnection;

    ScreenItem* m_screenItem = nullptr;
    const Screen* m_screen = nullptr;
    double m_zoom = 1.0;
};

} // namespace hmi
