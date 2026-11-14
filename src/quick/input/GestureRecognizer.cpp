#include "quick/input/GestureRecognizer.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QTouchEvent>
#include <QWheelEvent>

#include <algorithm>
#include <vector>

namespace hmi
{

namespace
{

PointerDevice deviceOf(const QPointerEvent& event)
{
    const QPointingDevice* device = event.pointingDevice();
    if (!device)
        return PointerDevice::Mouse;
    switch (device->type())
    {
    case QInputDevice::DeviceType::TouchScreen:
        return PointerDevice::Touch;
    case QInputDevice::DeviceType::Stylus:
    case QInputDevice::DeviceType::Airbrush:
        return PointerDevice::Pen;
    default:
        return PointerDevice::Mouse;
    }
}

double distance(QPointF a, QPointF b)
{
    return QLineF(a, b).length();
}

// A pinch distance below this is treated as this, so scale stays finite.
constexpr double kMinimumPinchDistance = 1.0;

} // namespace

GestureRecognizer::GestureRecognizer(const InputSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_longPressTimer.setSingleShot(true);
    connect(&m_longPressTimer, &QTimer::timeout, this, &GestureRecognizer::onLongPressTimeout);
}

bool GestureRecognizer::mousePress(const QMouseEvent& event)
{
    if (m_state == State::Idle)
        press(event.position(), deviceOf(event), event.button(), event.modifiers());
    return true;
}

bool GestureRecognizer::mouseMove(const QMouseEvent& event)
{
    move(event.position(), event.modifiers());
    return true;
}

bool GestureRecognizer::mouseRelease(const QMouseEvent& event)
{
    // Releasing a second button does not end the sequence started by the first.
    if (m_state != State::Idle && m_state != State::WaitRelease && event.button() != m_button)
        return true;
    release(event.position(), event.modifiers());
    return true;
}

bool GestureRecognizer::touch(const QTouchEvent& event)
{
    if (event.type() == QEvent::TouchCancel)
    {
        cancel();
        m_touchPoints.clear();
        m_state = State::Idle;
        return true;
    }

    const PointerDevice device =
        deviceOf(event) == PointerDevice::Pen ? PointerDevice::Pen : PointerDevice::Touch;
    // Positions of stationary points are not always meaningful, so each
    // finger's last reported position is kept by id.
    bool anyReleased = false;
    QPointF first = m_last;
    for (const QEventPoint& point : event.points())
    {
        if (point.state() == QEventPoint::Released)
        {
            anyReleased = true;
            first = point.position();
            m_touchPoints.erase(point.id());
        }
        else if (point.state() != QEventPoint::Stationary || !m_touchPoints.contains(point.id()))
        {
            m_touchPoints[point.id()] = point.position();
        }
    }
    if (event.type() == QEvent::TouchEnd)
        m_touchPoints.clear();
    std::vector<QPointF> down;
    for (const auto& [id, pos] : m_touchPoints)
        down.push_back(pos);
    if (down.size() == 1)
        first = down.front();

    switch (m_state)
    {
    case State::Idle:
        if (down.size() >= 2)
            beginPinch(down[0], down[1]);
        else if (down.size() == 1)
            press(down[0], device, Qt::LeftButton, event.modifiers());
        break;
    case State::Pressed:
    case State::LongPressed:
    case State::Dragging:
        if (down.size() >= 2)
        {
            m_longPressTimer.stop();
            if (m_state == State::Dragging)
                emitGesture(PointerGesture::Kind::DragCancel, m_last);
            beginPinch(down[0], down[1]);
        }
        else if (down.size() == 1)
        {
            move(down[0], event.modifiers());
        }
        else
        {
            release(first, event.modifiers());
        }
        break;
    case State::Pinching:
        if (down.size() >= 2 && !anyReleased)
        {
            updatePinch(down[0], down[1]);
        }
        else
        {
            PointerGesture g;
            g.kind = PointerGesture::Kind::PinchEnd;
            g.device = PointerDevice::Touch;
            g.viewPos = g.startViewPos = m_pinchCentre;
            Q_EMIT gesture(g);
            m_state = down.empty() ? State::Idle : State::WaitRelease;
        }
        break;
    case State::Panning:
    case State::WaitRelease:
        if (down.empty())
            m_state = State::Idle;
        break;
    }
    return true;
}

bool GestureRecognizer::wheel(const QWheelEvent& event)
{
    PointerGesture g;
    g.kind = PointerGesture::Kind::Wheel;
    g.device = PointerDevice::Mouse;
    g.viewPos = g.startViewPos = event.position();
    g.angleDelta = event.angleDelta();
    g.modifiers = event.modifiers();
    Q_EMIT gesture(g);
    return true;
}

void GestureRecognizer::cancel()
{
    m_longPressTimer.stop();
    switch (m_state)
    {
    case State::Dragging:
        emitGesture(PointerGesture::Kind::DragCancel, m_last);
        break;
    case State::Panning:
        emitGesture(PointerGesture::Kind::PanEnd, m_last);
        break;
    case State::Pinching:
    {
        PointerGesture g;
        g.kind = PointerGesture::Kind::PinchEnd;
        g.device = PointerDevice::Touch;
        g.viewPos = g.startViewPos = m_pinchCentre;
        Q_EMIT gesture(g);
        break;
    }
    default:
        break;
    }
    if (m_state != State::Idle)
        m_state = State::WaitRelease;
}

void GestureRecognizer::press(QPointF pos, PointerDevice device, Qt::MouseButton button,
                              Qt::KeyboardModifiers modifiers)
{
    m_device = device;
    m_button = button;
    m_modifiers = modifiers;
    m_start = m_last = pos;
    m_afterLongPress = false;

    const bool panButton =
        button == Qt::MiddleButton ||
        (button == Qt::LeftButton && m_panKeyHeld && device == PointerDevice::Mouse);
    if (panButton)
    {
        m_state = State::Panning;
        emitGesture(PointerGesture::Kind::PanBegin, pos);
        return;
    }

    m_state = State::Pressed;
    if (device != PointerDevice::Mouse && button == Qt::LeftButton)
        m_longPressTimer.start(m_settings.longPressMs);
}

void GestureRecognizer::move(QPointF pos, Qt::KeyboardModifiers modifiers)
{
    m_modifiers = modifiers;
    switch (m_state)
    {
    case State::Pressed:
    case State::LongPressed:
        if (distance(pos, m_start) < m_settings.dragDistance(m_device))
            return;
        m_longPressTimer.stop();
        if (m_button != Qt::LeftButton)
        {
            m_state = State::WaitRelease;
            return;
        }
        m_afterLongPress = m_state == State::LongPressed;
        m_state = State::Dragging;
        emitGesture(PointerGesture::Kind::DragBegin, m_start);
        emitGesture(PointerGesture::Kind::DragUpdate, pos);
        break;
    case State::Dragging:
        emitGesture(PointerGesture::Kind::DragUpdate, pos);
        break;
    case State::Panning:
        emitGesture(PointerGesture::Kind::PanUpdate, pos);
        break;
    default:
        break;
    }
}

void GestureRecognizer::release(QPointF pos, Qt::KeyboardModifiers modifiers)
{
    m_longPressTimer.stop();
    m_modifiers = modifiers;
    switch (m_state)
    {
    case State::Pressed:
        if (m_button == Qt::RightButton)
        {
            emitGesture(PointerGesture::Kind::ContextMenu, pos);
        }
        else if (m_button == Qt::LeftButton)
        {
            const bool isDouble = m_lastTapTime.isValid() &&
                                  m_lastTapTime.elapsed() < m_settings.doubleTapMs &&
                                  m_lastTapDevice == m_device &&
                                  distance(pos, m_lastTapPos) < m_settings.doubleTapDistance;
            if (isDouble)
            {
                m_lastTapTime.invalidate();
                emitGesture(PointerGesture::Kind::DoubleTap, pos);
            }
            else
            {
                m_lastTapTime.start();
                m_lastTapPos = pos;
                m_lastTapDevice = m_device;
                emitGesture(PointerGesture::Kind::Tap, pos);
            }
        }
        break;
    case State::Dragging:
        emitGesture(PointerGesture::Kind::DragEnd, pos);
        break;
    case State::Panning:
        emitGesture(PointerGesture::Kind::PanEnd, pos);
        break;
    default:
        break;
    }
    m_state = State::Idle;
}

void GestureRecognizer::beginPinch(QPointF a, QPointF b)
{
    m_state = State::Pinching;
    m_pinchCentre = (a + b) / 2.0;
    m_pinchDistance = std::max(distance(a, b), kMinimumPinchDistance);

    PointerGesture g;
    g.kind = PointerGesture::Kind::PinchBegin;
    g.device = PointerDevice::Touch;
    g.viewPos = g.startViewPos = m_pinchCentre;
    Q_EMIT gesture(g);
}

void GestureRecognizer::updatePinch(QPointF a, QPointF b)
{
    const QPointF centre = (a + b) / 2.0;
    const double span = std::max(distance(a, b), kMinimumPinchDistance);

    PointerGesture g;
    g.kind = PointerGesture::Kind::PinchUpdate;
    g.device = PointerDevice::Touch;
    g.viewPos = centre;
    g.startViewPos = m_pinchCentre;
    g.viewDelta = centre - m_pinchCentre;
    g.scaleDelta = span / m_pinchDistance;

    m_pinchCentre = centre;
    m_pinchDistance = span;
    Q_EMIT gesture(g);
}

void GestureRecognizer::onLongPressTimeout()
{
    if (m_state != State::Pressed)
        return;
    m_state = State::LongPressed;
    emitGesture(PointerGesture::Kind::LongPress, m_start);
}

void GestureRecognizer::emitGesture(PointerGesture::Kind kind, QPointF pos)
{
    PointerGesture g;
    g.kind = kind;
    g.device = m_device;
    g.viewPos = pos;
    g.startViewPos = m_start;
    g.viewDelta = pos - m_last;
    g.modifiers = m_modifiers;
    g.afterLongPress = m_afterLongPress;
    m_last = pos;
    Q_EMIT gesture(g);
}

} // namespace hmi
