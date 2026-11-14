#include "core/spatial/Snapper.h"

#include "core/model/Screen.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Rectangle.h"

#include <QTest>

using namespace hmi;

namespace
{

constexpr double kThreshold = 6.0;

Snapper::Settings gridOnly()
{
    return {.toGrid = true, .toObjects = false, .gridSpacing = 10.0};
}

Snapper::Settings objectsOnly()
{
    return {.toGrid = false, .toObjects = true, .gridSpacing = 10.0};
}

} // namespace

class SnapperTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Screen> m_screen;
    ObjectId m_moving;

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.size({1000, 600});
        builder.add(Rectangle::kTypeId, {100, 100, 100, 50}); // anchor object
        builder.add(Rectangle::kTypeId, {400, 300, 40, 40});
        m_moving = builder.lastId();
        m_screen = builder.build();
    }

    void gridSnapsEdges()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), gridOnly());
        const SnapResult r = snapper.snapMove({403, 297, 40, 40}, kThreshold);
        QCOMPARE(r.offset, QPointF(-3, 3));
        QVERIFY(r.guides.empty()); // the grid draws no guides

        // Right edge on a grid line when the left edge is further away.
        const SnapResult right = snapper.snapMove({405, 300, 33, 40}, 2.5);
        QCOMPARE(right.offset.x(), 2.0);
    }

    void outsideThresholdDoesNothing()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), gridOnly());
        QCOMPARE(snapper.snapMove({405, 305, 40, 40}, 2.0).offset, QPointF());
    }

    void objectsSnapEdgesAndCentresWithGuides()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), objectsOnly());
        // Left edge 4 px right of the anchor's right edge (200).
        SnapResult r = snapper.snapMove({204, 320, 40, 40}, kThreshold);
        QCOMPARE(r.offset.x(), -4.0);
        QCOMPARE(r.guides.size(), std::size_t(1));
        QCOMPARE(r.guides[0].x1(), 200.0);
        QCOMPARE(r.guides[0].y1(), 100.0); // from the anchor's top ...
        QCOMPARE(r.guides[0].y2(), 360.0); // ... to the moving rect's bottom

        // Centre lines up with the anchor's centre (150, 125).
        r = snapper.snapMove({128, 103, 40, 40}, kThreshold);
        QCOMPARE(r.offset, QPointF(2, 2));
        QCOMPARE(r.guides.size(), std::size_t(2));
    }

    void screenEdgesAreTargets()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), objectsOnly());
        QCOMPARE(snapper.snapMove({3, 556, 40, 40}, kThreshold).offset, QPointF(-3, 4));
    }

    void objectsBeatGridOnTies()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1),
                              {.toGrid = true, .toObjects = true, .gridSpacing = 7.0});
        // 203 is 3 from the object edge 200 and 7 * 29 = 203 is a grid line;
        // the grid is closer, so it wins.
        QCOMPARE(snapper.snapMove({203, 300, 40, 40}, kThreshold).offset.x(), 0.0);
        // 202: object 2 away, grid line 203 1 away; grid wins again.
        QCOMPARE(snapper.snapMove({202, 300, 40, 40}, kThreshold).offset.x(), 1.0);
        // 201.5: object 1.5 away, grid 1.5 away; the object wins the tie.
        QCOMPARE(snapper.snapMove({201.5, 300, 40, 40}, kThreshold).offset.x(), -1.5);
    }

    void movingObjectsAreNotTargets()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), objectsOnly());
        // The moving object's own start position (400, 300) is not a target.
        QCOMPARE(snapper.snapMove({402, 322, 40, 40}, kThreshold).offset, QPointF());
    }

    void resizeSnapsOnlyMovingEdges()
    {
        const Snapper snapper(*m_screen, std::span(&m_moving, 1), objectsOnly());
        // Dragging the right edge to 197: snaps to the anchor's right edge.
        const SnapResult r = snapper.snapResize({150, 300, 47, 40}, {.right = true}, kThreshold);
        QCOMPARE(r.offset, QPointF(3, 0));
        QCOMPARE(r.guides.size(), std::size_t(1));
        // The left edge (150 = anchor centre) is not moving, so no snap there.
        const SnapResult none =
            snapper.snapResize({152, 300, 100, 40}, {.right = true}, kThreshold);
        QCOMPARE(none.offset, QPointF());
    }
};

QTEST_MAIN(SnapperTest)
#include "SnapperTest.moc"
