#include "core/spatial/HitTester.h"

#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"

#include <QTest>

using namespace hmi;

class HitTesterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void topmostWins()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {0, 0, 100, 100});
        const ObjectId bottom = builder.lastId();
        builder.add(Rectangle::kTypeId, {50, 50, 100, 100});
        const ObjectId top = builder.lastId();
        const auto screen = builder.build();

        QCOMPARE(HitTester::topmostAt(*screen, {75, 75})->id(), top);
        QCOMPARE(HitTester::topmostAt(*screen, {25, 25})->id(), bottom);
        QVERIFY(HitTester::topmostAt(*screen, {200, 200}) == nullptr);

        const auto all = HitTester::allAt(*screen, {75, 75});
        QCOMPARE(all.size(), std::size_t(2));
        QCOMPARE(all[0]->id(), top);
        QCOMPARE(all[1]->id(), bottom);
    }

    void respectsHitShape()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {0, 0, 100, 100});
        const ObjectId rect = builder.lastId();
        builder.add(Ellipse::kTypeId, {0, 0, 100, 100});
        const ObjectId ellipse = builder.lastId();
        const auto screen = builder.build();

        // The ellipse is on top, but its empty corner falls through.
        QCOMPARE(HitTester::topmostAt(*screen, {50, 50})->id(), ellipse);
        QCOMPARE(HitTester::topmostAt(*screen, {3, 3})->id(), rect);
    }

    void toleranceExtendsTheShape()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Line::kTypeId, {0, 50, 100, 0}).with(Prop::LineWidth, 1.0);
        const auto screen = builder.build();
        const HmiObject& line = *screen->objects().front();

        QVERIFY(HitTester::hits(line, {50, 50}, 0.0));
        QVERIFY(!HitTester::hits(line, {50, 55}, 0.0));
        QVERIFY(!HitTester::hits(line, {50, 55}, 3.0));
        QVERIFY(HitTester::hits(line, {50, 55}, 6.0));  // touch-sized tolerance
        QVERIFY(HitTester::hits(line, {103, 50}, 4.0)); // past the end, rounded cap
        QVERIFY(!HitTester::hits(line, {110, 50}, 4.0));
    }

    void rotationIsApplied()
    {
        // A long, thin bar rotated 90 degrees about its centre (50, 5)
        // becomes vertical.
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {0, 0, 100, 10}).with(Prop::Rotation, 90.0);
        const auto screen = builder.build();
        const HmiObject& bar = *screen->objects().front();

        QVERIFY(HitTester::hits(bar, {50, 40}, 0.0));
        QVERIFY(HitTester::hits(bar, {50, -30}, 0.0));
        QVERIFY(!HitTester::hits(bar, {90, 5}, 0.0));
        QVERIFY(bar.sceneBoundingRect().contains(QRectF(46, -44, 8, 98)));
    }

    void skipsHiddenAndOptionallyLocked()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {0, 0, 100, 100});
        const ObjectId bottom = builder.lastId();
        builder.add(Rectangle::kTypeId, {0, 0, 100, 100}).with(Prop::Locked, true);
        const ObjectId locked = builder.lastId();
        builder.add(Rectangle::kTypeId, {0, 0, 100, 100}).with(Prop::Visible, false);
        const ObjectId hidden = builder.lastId();
        const auto screen = builder.build();

        QCOMPARE(HitTester::topmostAt(*screen, {50, 50})->id(), locked);
        QCOMPARE(HitTester::topmostAt(*screen, {50, 50}, {.includeLocked = false})->id(), bottom);
        QCOMPARE(HitTester::topmostAt(*screen, {50, 50}, {.includeHidden = true})->id(), hidden);
    }

    void rubberBandModes()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.add(Rectangle::kTypeId, {10, 10, 30, 30});
        const ObjectId inside = builder.lastId();
        builder.add(Rectangle::kTypeId, {80, 10, 40, 30});
        const ObjectId straddling = builder.lastId();
        builder.add(Ellipse::kTypeId, {0, 80, 40, 40});
        const ObjectId ellipse = builder.lastId();
        const auto screen = builder.build();

        const QRectF band(0, 0, 100, 50);
        const auto contained = HitTester::inRect(*screen, band, HitTester::RectMode::Contains);
        QCOMPARE(contained.size(), std::size_t(1));
        QCOMPARE(contained[0]->id(), inside);

        const auto touched = HitTester::inRect(*screen, band, HitTester::RectMode::Intersects);
        QCOMPARE(touched.size(), std::size_t(2));
        QCOMPARE(touched[0]->id(), inside);
        QCOMPARE(touched[1]->id(), straddling);

        // A band over the ellipse's empty bounding-box corner misses it.
        const QRectF corner(0, 80, 5, 5);
        QVERIFY(HitTester::inRect(*screen, corner, HitTester::RectMode::Intersects).empty());
        // Reversed (negative size) bands from dragging up-left work too.
        const auto reversed =
            HitTester::inRect(*screen, QRectF(45, 125, -50, -50), HitTester::RectMode::Contains);
        QCOMPARE(reversed.size(), std::size_t(1));
        QCOMPARE(reversed[0]->id(), ellipse);
    }
};

QTEST_MAIN(HitTesterTest)
#include "HitTesterTest.moc"
