#pragma once

#include "quick/input/PointerGesture.h"

#include <QTest>

#include <vector>

namespace hmi::test
{

// Builds gesture sequences as the recognizer would emit them, with view and
// scene coordinates equal (zoom 1, no pan).
struct GestureFactory
{
    PointerDevice device = PointerDevice::Mouse;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    [[nodiscard]] PointerGesture make(PointerGesture::Kind kind, QPointF start, QPointF pos,
                                      QPointF delta = {}, bool afterLongPress = false) const
    {
        PointerGesture g;
        g.kind = kind;
        g.device = device;
        g.viewPos = g.scenePos = pos;
        g.startViewPos = g.startScenePos = start;
        g.viewDelta = delta;
        g.modifiers = modifiers;
        g.afterLongPress = afterLongPress;
        return g;
    }

    [[nodiscard]] PointerGesture tap(QPointF pos) const
    {
        return make(PointerGesture::Kind::Tap, pos, pos);
    }

    [[nodiscard]] PointerGesture longPress(QPointF pos) const
    {
        return make(PointerGesture::Kind::LongPress, pos, pos);
    }

    // Begin, one update per step and End, from `from` to `to`.
    [[nodiscard]] std::vector<PointerGesture> drag(QPointF from, QPointF to, int steps = 3,
                                                   bool afterLongPress = false) const
    {
        using Kind = PointerGesture::Kind;
        std::vector<PointerGesture> sequence;
        sequence.push_back(make(Kind::DragBegin, from, from, {}, afterLongPress));
        QPointF previous = from;
        for (int i = 1; i <= steps; ++i)
        {
            const QPointF p = from + (to - from) * (double(i) / steps);
            sequence.push_back(make(Kind::DragUpdate, from, p, p - previous, afterLongPress));
            previous = p;
        }
        sequence.push_back(make(Kind::DragEnd, from, to, {}, afterLongPress));
        return sequence;
    }
};

// Test rows for running a tool test once with mouse and once with touch.
inline void addDeviceRows()
{
    QTest::addColumn<PointerDevice>("device");
    QTest::newRow("mouse") << PointerDevice::Mouse;
    QTest::newRow("touch") << PointerDevice::Touch;
}

} // namespace hmi::test

Q_DECLARE_METATYPE(hmi::PointerDevice)
