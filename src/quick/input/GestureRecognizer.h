#pragma once

#include "quick/input/InputSettings.h"
#include "quick/input/PointerGesture.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QTimer>

#include <map>

class QMouseEvent;
class QTouchEvent;
class QWheelEvent;

namespace hmi
{

// Turns mouse, touch, pen and wheel events into PointerGestures (Adapter and
// State pattern). One primary pointer drives tap, long press and drag; a
// second finger turns the sequence into a pinch, cancelling any drag in
// progress. Middle button, or left button while the pan key is held, pans.
//
// Positions are taken from the events as delivered to the owning item, so
// gestures carry view coordinates; scene coordinates are added by the item.
class GestureRecognizer : public QObject
{
    Q_OBJECT

public:
    explicit GestureRecognizer(const InputSettings& settings, QObject* parent = nullptr);

    // Each returns true when the event was used.
    bool mousePress(const QMouseEvent& event);
    bool mouseMove(const QMouseEvent& event);
    bool mouseRelease(const QMouseEvent& event);
    bool touch(const QTouchEvent& event);
    bool wheel(const QWheelEvent& event);

    // Held pan key (Space): a left drag pans instead of dragging.
    void setPanKeyHeld(bool held) noexcept { m_panKeyHeld = held; }

    // Ends the current sequence: a drag emits DragCancel, a pan or pinch its
    // end. Further events of the sequence are ignored until all pointers are
    // released.
    void cancel();

    // True while a pointer sequence is in progress.
    [[nodiscard]] bool isActive() const noexcept { return m_state != State::Idle; }

Q_SIGNALS:
    void gesture(const hmi::PointerGesture& gesture);

private:
    enum class State
    {
        Idle,
        Pressed,     // primary pointer down, not moved past the threshold
        LongPressed, // long press emitted, waiting for a drag or release
        Dragging,
        Panning,
        Pinching,
        WaitRelease, // sequence ended or cancelled, ignore until release
    };

    void press(QPointF pos, PointerDevice device, Qt::MouseButton button,
               Qt::KeyboardModifiers modifiers);
    void move(QPointF pos, Qt::KeyboardModifiers modifiers);
    void release(QPointF pos, Qt::KeyboardModifiers modifiers);
    void beginPinch(QPointF a, QPointF b);
    void updatePinch(QPointF a, QPointF b);
    void onLongPressTimeout();
    void emitGesture(PointerGesture::Kind kind, QPointF pos);

    const InputSettings& m_settings;
    State m_state = State::Idle;
    PointerDevice m_device = PointerDevice::Mouse;
    Qt::MouseButton m_button = Qt::NoButton;
    Qt::KeyboardModifiers m_modifiers = Qt::NoModifier;
    QPointF m_start;
    QPointF m_last;
    bool m_afterLongPress = false;
    bool m_panKeyHeld = false;

    QPointF m_pinchCentre;
    double m_pinchDistance = 0.0;
    std::map<int, QPointF> m_touchPoints; // fingers down, by point id

    QTimer m_longPressTimer;
    QElapsedTimer m_lastTapTime;
    QPointF m_lastTapPos;
    PointerDevice m_lastTapDevice = PointerDevice::Mouse;
};

} // namespace hmi
