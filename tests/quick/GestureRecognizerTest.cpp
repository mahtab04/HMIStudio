// Feeds synthetic mouse, touch, wheel and key events through a real
// QQuickWindow to a CanvasItem and checks the gestures it emits.

#include "quick/CanvasItem.h"

#include <QPointingDevice>
#include <QQuickWindow>
#include <QTest>
#include <QWheelEvent>

#include <memory>
#include <vector>

using namespace hmi;
using Kind = PointerGesture::Kind;

class GestureRecognizerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QQuickWindow> m_window;
    CanvasItem* m_canvas = nullptr;
    QPointingDevice* m_touch = nullptr;
    std::vector<PointerGesture> m_gestures;

    [[nodiscard]] std::vector<Kind> kinds() const
    {
        std::vector<Kind> result;
        for (const PointerGesture& g : m_gestures)
            result.push_back(g.kind);
        return result;
    }

    [[nodiscard]] const PointerGesture* last(Kind kind) const
    {
        for (auto it = m_gestures.rbegin(); it != m_gestures.rend(); ++it)
        {
            if (it->kind == kind)
                return &*it;
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase() { m_touch = QTest::createTouchDevice(); }

    void init()
    {
        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(400, 300);
        m_canvas = new CanvasItem(m_window->contentItem());
        m_canvas->setSize(QSizeF(400, 300));
        m_canvas->forceActiveFocus();
        m_gestures.clear();
        connect(m_canvas, &CanvasItem::gesture, this,
                [this](const PointerGesture& g) { m_gestures.push_back(g); });
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup() { m_window.reset(); }

    void mouseClickIsTap()
    {
        QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, QPoint(50, 60));
        QCOMPARE(kinds(), std::vector{Kind::Tap});
        QCOMPARE(m_gestures[0].device, PointerDevice::Mouse);
        QCOMPARE(m_gestures[0].viewPos, QPointF(50, 60));
    }

    void smallMoveIsStillTap()
    {
        QTest::mousePress(m_window.get(), Qt::LeftButton, {}, QPoint(50, 50));
        QTest::mouseMove(m_window.get(), QPoint(52, 51));
        QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, QPoint(52, 51));
        QCOMPARE(kinds(), std::vector{Kind::Tap});
    }

    void doubleClickIsDoubleTap()
    {
        QTest::mouseDClick(m_window.get(), Qt::LeftButton, {}, QPoint(80, 80));
        QVERIFY(last(Kind::Tap));
        QVERIFY(last(Kind::DoubleTap));
    }

    void mouseDrag()
    {
        QTest::mousePress(m_window.get(), Qt::LeftButton, Qt::ShiftModifier, QPoint(50, 50));
        QTest::mouseMove(m_window.get(), QPoint(70, 50));
        QTest::mouseMove(m_window.get(), QPoint(90, 60));
        QTest::mouseRelease(m_window.get(), Qt::LeftButton, Qt::ShiftModifier, QPoint(90, 60));

        QCOMPARE(kinds(),
                 (std::vector{Kind::DragBegin, Kind::DragUpdate, Kind::DragUpdate, Kind::DragEnd}));
        QCOMPARE(m_gestures.front().viewPos, QPointF(50, 50));
        QCOMPARE(m_gestures[2].viewDelta, QPointF(20, 10));
        QCOMPARE(m_gestures.back().startViewPos, QPointF(50, 50));
        QVERIFY(m_gestures.back().modifiers.testFlag(Qt::ShiftModifier));
    }

    void scenePositionsFollowZoomAndPan()
    {
        m_canvas->zoomAt(QPointF(0, 0), 2.0);
        m_canvas->panBy(QPointF(10, 20));
        QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, QPoint(110, 220));
        QCOMPARE(m_gestures.back().scenePos, QPointF(50, 100));
    }

    void rightClickIsContextMenu()
    {
        QTest::mouseClick(m_window.get(), Qt::RightButton, {}, QPoint(40, 40));
        QCOMPARE(kinds(), std::vector{Kind::ContextMenu});
    }

    void middleDragPans()
    {
        QTest::mousePress(m_window.get(), Qt::MiddleButton, {}, QPoint(50, 50));
        QTest::mouseMove(m_window.get(), QPoint(60, 55));
        QTest::mouseRelease(m_window.get(), Qt::MiddleButton, {}, QPoint(60, 55));
        QCOMPARE(kinds(), (std::vector{Kind::PanBegin, Kind::PanUpdate, Kind::PanEnd}));
        QCOMPARE(m_gestures[1].viewDelta, QPointF(10, 5));
    }

    void spaceDragPans()
    {
        QTest::keyPress(m_window.get(), Qt::Key_Space);
        QTest::mousePress(m_window.get(), Qt::LeftButton, {}, QPoint(50, 50));
        QTest::mouseMove(m_window.get(), QPoint(70, 50));
        QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, QPoint(70, 50));
        QTest::keyRelease(m_window.get(), Qt::Key_Space);
        QCOMPARE(kinds().front(), Kind::PanBegin);
        QCOMPARE(kinds().back(), Kind::PanEnd);
    }

    void escapeCancelsDrag()
    {
        QTest::mousePress(m_window.get(), Qt::LeftButton, {}, QPoint(50, 50));
        QTest::mouseMove(m_window.get(), QPoint(80, 50));
        QTest::keyClick(m_window.get(), Qt::Key_Escape);
        QTest::mouseMove(m_window.get(), QPoint(90, 50));
        QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, QPoint(90, 50));
        QCOMPARE(kinds(), (std::vector{Kind::DragBegin, Kind::DragUpdate, Kind::DragCancel}));
    }

    void wheelCarriesAngleAndModifiers()
    {
        QWheelEvent wheel(QPointF(100, 100), m_window->mapToGlobal(QPointF(100, 100)), QPoint(),
                          QPoint(0, 120), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase,
                          false);
        QCoreApplication::sendEvent(m_window.get(), &wheel);
        QCOMPARE(kinds(), std::vector{Kind::Wheel});
        QCOMPARE(m_gestures[0].angleDelta, QPoint(0, 120));
        QVERIFY(m_gestures[0].modifiers.testFlag(Qt::ControlModifier));
    }

    void touchTap()
    {
        QTest::touchEvent(m_window.get(), m_touch).press(0, QPoint(60, 60));
        QTest::touchEvent(m_window.get(), m_touch).release(0, QPoint(62, 61));
        QCOMPARE(kinds(), std::vector{Kind::Tap});
        QCOMPARE(m_gestures[0].device, PointerDevice::Touch);
    }

    void touchJitterBelowThresholdIsTap()
    {
        // 8 px is a drag for a mouse but not for a finger.
        QTest::touchEvent(m_window.get(), m_touch).press(0, QPoint(60, 60));
        QTest::touchEvent(m_window.get(), m_touch).move(0, QPoint(68, 60));
        QTest::touchEvent(m_window.get(), m_touch).release(0, QPoint(68, 60));
        QCOMPARE(kinds(), std::vector{Kind::Tap});
    }

    void touchDrag()
    {
        QTest::touchEvent(m_window.get(), m_touch).press(0, QPoint(60, 60));
        QTest::touchEvent(m_window.get(), m_touch).move(0, QPoint(100, 60));
        QTest::touchEvent(m_window.get(), m_touch).release(0, QPoint(100, 60));
        QCOMPARE(kinds(), (std::vector{Kind::DragBegin, Kind::DragUpdate, Kind::DragEnd}));
        QVERIFY(!m_gestures[0].afterLongPress);
        QVERIFY(m_gestures[0].isTouch());
    }

    void touchLongPressThenDrag()
    {
        QTest::touchEvent(m_window.get(), m_touch).press(0, QPoint(60, 60));
        QTRY_VERIFY(last(Kind::LongPress));
        QTest::touchEvent(m_window.get(), m_touch).move(0, QPoint(120, 90));
        QTest::touchEvent(m_window.get(), m_touch).release(0, QPoint(120, 90));
        QCOMPARE(kinds(),
                 (std::vector{Kind::LongPress, Kind::DragBegin, Kind::DragUpdate, Kind::DragEnd}));
        QVERIFY(last(Kind::DragEnd)->afterLongPress);
    }

    void mouseHasNoLongPress()
    {
        QTest::mousePress(m_window.get(), Qt::LeftButton, {}, QPoint(60, 60));
        QTest::qWait(700);
        QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, QPoint(60, 60));
        QCOMPARE(kinds(), std::vector{Kind::Tap});
    }

    void pinch()
    {
        QTest::touchEvent(m_window.get(), m_touch)
            .press(0, QPoint(100, 100))
            .press(1, QPoint(200, 100));
        QTest::touchEvent(m_window.get(), m_touch)
            .move(0, QPoint(50, 100))
            .move(1, QPoint(250, 100));
        QTest::touchEvent(m_window.get(), m_touch)
            .release(0, QPoint(50, 100))
            .release(1, QPoint(250, 100));

        QCOMPARE(kinds(), (std::vector{Kind::PinchBegin, Kind::PinchUpdate, Kind::PinchEnd}));
        const PointerGesture& update = m_gestures[1];
        QCOMPARE(update.scaleDelta, 2.0);
        QCOMPARE(update.viewPos, QPointF(150, 100));
        QCOMPARE(update.viewDelta, QPointF(0, 0));
    }

    void secondFingerCancelsDrag()
    {
        QTest::touchEvent(m_window.get(), m_touch).press(0, QPoint(60, 60));
        QTest::touchEvent(m_window.get(), m_touch).move(0, QPoint(100, 60));
        QTest::touchEvent(m_window.get(), m_touch).stationary(0).press(1, QPoint(200, 60));
        QTest::touchEvent(m_window.get(), m_touch).move(0, QPoint(90, 70)).move(1, QPoint(210, 70));
        QTest::touchEvent(m_window.get(), m_touch)
            .release(0, QPoint(90, 70))
            .release(1, QPoint(210, 70));

        QCOMPARE(kinds(), (std::vector{Kind::DragBegin, Kind::DragUpdate, Kind::DragCancel,
                                       Kind::PinchBegin, Kind::PinchUpdate, Kind::PinchEnd}));
        QCOMPARE(m_gestures[4].viewDelta, QPointF(0, 10));
    }
};

QTEST_MAIN(GestureRecognizerTest)
#include "GestureRecognizerTest.moc"
