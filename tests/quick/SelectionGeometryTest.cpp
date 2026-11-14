#include "quick/overlays/SelectionGeometry.h"

#include "core/model/HmiObject.h"
#include "quick/overlays/RulerItem.h"

#include <QTest>

using namespace hmi;

namespace
{

bool near(QPointF a, QPointF b, double tolerance = 1e-6)
{
    return std::abs(a.x() - b.x()) < tolerance && std::abs(a.y() - b.y()) < tolerance;
}

} // namespace

class SelectionGeometryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void handlesOfUnrotatedRect()
    {
        const auto h = SelectionGeometry::handles({10, 20, 100, 50}, 0.0);
        QCOMPARE(h[std::size_t(SelectionHandle::TopLeft)], QPointF(10, 20));
        QCOMPARE(h[std::size_t(SelectionHandle::Top)], QPointF(60, 20));
        QCOMPARE(h[std::size_t(SelectionHandle::BottomRight)], QPointF(110, 70));
        QCOMPARE(h[std::size_t(SelectionHandle::Left)], QPointF(10, 45));
    }

    void handleHitUsesSquareArea()
    {
        const QRectF g(0, 0, 100, 100);
        QCOMPARE(SelectionGeometry::handleAt(g, 0, {103, 104}, 12.0),
                 std::optional(SelectionHandle::BottomRight));
        QCOMPARE(SelectionGeometry::handleAt(g, 0, {108, 100}, 12.0), std::nullopt);
        // A touch-sized area reaches further.
        QCOMPARE(SelectionGeometry::handleAt(g, 0, {118, 100}, 44.0),
                 std::optional(SelectionHandle::BottomRight));
    }

    void resizeFromEachSide()
    {
        const QRectF g(10, 10, 100, 50);
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::BottomRight, {20, 10}),
                 QRectF(10, 10, 120, 60));
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::TopLeft, {-5, -5}),
                 QRectF(5, 5, 105, 55));
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::Right, {20, 99}),
                 QRectF(10, 10, 120, 50));
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::Top, {99, 20}),
                 QRectF(10, 30, 100, 30));
    }

    void resizeStopsAtMinimumSize()
    {
        const QRectF g(10, 10, 100, 50);
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::Right, {-500, 0}),
                 QRectF(10, 10, 1, 50));
        QCOMPARE(SelectionGeometry::resized(g, 0, SelectionHandle::Left, {500, 0}),
                 QRectF(109, 10, 1, 50));
    }

    void zeroHeightLineCanStayFlat()
    {
        const QRectF line(0, 50, 100, 0);
        QCOMPARE(SelectionGeometry::resized(line, 0, SelectionHandle::Right, {20, 0}),
                 QRectF(0, 50, 120, 0));
    }

    void rotatedResizeKeepsOppositeCornerFixed()
    {
        const QRectF g(100, 100, 80, 40);
        const double rotation = 30.0;
        const QPointF anchorBefore =
            SelectionGeometry::handles(g, rotation)[std::size_t(SelectionHandle::TopLeft)];

        const QRectF r =
            SelectionGeometry::resized(g, rotation, SelectionHandle::BottomRight, {25, 15});
        const QPointF anchorAfter =
            SelectionGeometry::handles(r, rotation)[std::size_t(SelectionHandle::TopLeft)];
        QVERIFY(near(anchorBefore, anchorAfter));
        QVERIFY(r.width() > g.width());
    }

    void frameFollowsRotation()
    {
        const QPolygonF frame = SelectionGeometry::frame({0, 0, 100, 10}, 90.0);
        QVERIFY(near(frame[0], QPointF(55, -45)));
        QVERIFY(near(frame[2], QPointF(45, 55)));
    }

    void rulerLabelSteps_data()
    {
        QTest::addColumn<double>("zoom");
        QTest::addColumn<double>("step");
        QTest::newRow("100%") << 1.0 << 100.0;
        QTest::newRow("50%") << 0.5 << 200.0;
        QTest::newRow("10%") << 0.1 << 1000.0;
        QTest::newRow("800%") << 8.0 << 10.0;
        QTest::newRow("150%") << 1.5 << 50.0;
    }

    void rulerLabelSteps()
    {
        QFETCH(double, zoom);
        QFETCH(double, step);
        QCOMPARE(RulerItem::labelStep(zoom), step);
        QVERIFY(RulerItem::labelStep(zoom) * zoom >= RulerItem::kMinimumLabelSpacing);
    }
};

QTEST_MAIN(SelectionGeometryTest)
#include "SelectionGeometryTest.moc"
