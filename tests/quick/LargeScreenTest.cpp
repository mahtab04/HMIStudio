// Performance and memory check with 2,000 objects on one screen (PLAN.md
// phase 3). Runs offscreen on the software renderer, the slowest backend, so
// the times are an upper bound. Prints the measurements; fails only on
// generous limits, so it catches regressions without being flaky.

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"
#include "core/property/Prop.h"
#include "core/spatial/HitTester.h"
#include "quick/CanvasItem.h"
#include "quick/ObjectItem.h"
#include "quick/ScreenItem.h"

#include <QElapsedTimer>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace hmi;

namespace
{

constexpr int kColumns = 50;
constexpr int kRows = 40;
constexpr double kPitchX = 38.0;
constexpr double kPitchY = 27.0;

// Bytes of all object textures (4 bytes per pixel).
qint64 textureBytes(const ScreenItem& screenItem, const Screen& screen)
{
    qint64 bytes = 0;
    for (const auto& object : screen.objects())
    {
        if (const ObjectItem* item = screenItem.objectItem(object->id()))
            bytes += qint64(item->textureSize().width()) * item->textureSize().height() * 4;
    }
    return bytes;
}

double mib(qint64 bytes)
{
    return double(bytes) / (1024.0 * 1024.0);
}

} // namespace

class LargeScreenTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    CanvasItem* m_canvas = nullptr;

    // Renders one frame and returns its time in milliseconds.
    double frame()
    {
        QElapsedTimer timer;
        timer.start();
        const QImage image = m_window->grabWindow();
        Q_UNUSED(image);
        return double(timer.nsecsElapsed()) / 1e6;
    }

    void settleZoom()
    {
        QSignalSpy settled(m_canvas, &CanvasItem::zoomSettled);
        QVERIFY(settled.wait(2000));
    }

private Q_SLOTS:
    void initTestCase()
    {
        const ObjectTypeId types[] = {Rectangle::kTypeId, Ellipse::kTypeId, Line::kTypeId,
                                      TextBox::kTypeId};
        ScreenBuilder builder(QStringLiteral("Large"));
        builder.size({1920, 1080});
        for (int row = 0; row < kRows; ++row)
        {
            for (int column = 0; column < kColumns; ++column)
            {
                builder.add(types[(row + column) % 4],
                            {10 + column * kPitchX, 10 + row * kPitchY, 30, 20});
            }
        }
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());
        QCOMPARE(m_screen->objectCount(), std::size_t(kColumns * kRows));

        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(1280, 800);
        m_canvas = new CanvasItem(m_window->contentItem());
        m_canvas->setSize(QSizeF(1280, 800));

        QElapsedTimer timer;
        timer.start();
        m_canvas->setScreen(m_screen, &m_project->notifier());
        const double buildMs = double(timer.elapsed());
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        const double firstFrame = frame();
        qInfo("2000 objects: items built in %.0f ms, first frame %.0f ms", buildMs, firstFrame);
        QVERIFY(buildMs < 2000);
    }

    void cleanupTestCase()
    {
        m_window.reset();
        m_project.reset();
    }

    void panAndZoomFrames()
    {
        m_canvas->fitScreen();
        settleZoom();
        frame(); // textures at the fitted zoom

        double pan = 0.0;
        constexpr int kFrames = 10;
        for (int i = 0; i < kFrames; ++i)
        {
            m_canvas->panBy({7, 3});
            pan += frame();
        }
        double zoom = 0.0;
        for (int i = 0; i < kFrames; ++i)
        {
            m_canvas->zoomAt({640, 400}, 1.02);
            zoom += frame();
        }
        qInfo("pan frame %.1f ms, zoom frame (before settling) %.1f ms", pan / kFrames,
              zoom / kFrames);
        // The software renderer at 1280 x 800; a GPU is far faster.
        QVERIFY(pan / kFrames < 250);
        QVERIFY(zoom / kFrames < 250);
    }

    void textureMemoryWithinBudget_data()
    {
        QTest::addColumn<double>("zoom");
        QTest::newRow("fit") << 0.0;
        QTest::newRow("100%") << 1.0;
        QTest::newRow("400%") << 4.0;
        QTest::newRow("800%") << 8.0;
    }
    void textureMemoryWithinBudget()
    {
        QFETCH(double, zoom);
        if (zoom == 0.0)
            m_canvas->fitScreen();
        else
            m_canvas->setZoomCentered(zoom);
        settleZoom();
        QElapsedTimer timer;
        timer.start();
        frame();
        const double rerender = double(timer.elapsed());
        const double used = mib(textureBytes(*m_canvas->screenItem(), *m_screen));
        qInfo("zoom %.0f%%: textures %.1f MiB, re-render after settling %.0f ms",
              m_canvas->zoom() * 100.0, used, rerender);
        // Budget: 64 MiB of object textures on one screen. Only objects near
        // the view render above 1:1, so deep zoom stays cheap.
        QVERIFY2(used < 64.0, qPrintable(QStringLiteral("%1 MiB").arg(used)));
    }

    void objectsPannedIntoViewBecomeSharp()
    {
        m_canvas->fitScreen();
        settleZoom();
        m_canvas->setZoomCentered(8.0);
        settleZoom();
        const auto textureWidthAt = [this](QPointF viewPos)
        {
            const QPointF scene = m_canvas->mapViewToScene(viewPos);
            const HmiObject* object = HitTester::topmostAt(*m_screen, scene, {.tolerance = 20});
            return object ? m_canvas->screenItem()->objectItem(object->id())->textureSize().width()
                          : 0;
        };
        // An object far right of the view renders at 1:1 (30 px wide) ...
        const QPointF farAway = m_canvas->mapSceneToView({1800, 500});
        QCOMPARE(textureWidthAt(farAway), 30);
        // ... until it is panned into view and the view settles.
        m_canvas->panBy(QPointF(640, 400) - farAway);
        settleZoom();
        QVERIFY(textureWidthAt({640, 400}) >= 30 * 8);
    }

    void selectAllAndHitTest()
    {
        m_canvas->fitScreen();
        std::vector<ObjectId> all;
        for (const auto& object : m_screen->objects())
            all.push_back(object->id());
        QElapsedTimer timer;
        timer.start();
        m_canvas->setSelection(all);
        const double select = double(timer.nsecsElapsed()) / 1e6;
        const double selectedFrame = frame();

        timer.restart();
        constexpr int kHits = 1000;
        int found = 0;
        for (int i = 0; i < kHits; ++i)
        {
            const QPointF p(10 + (i % kColumns) * kPitchX + 5, 10 + (i % kRows) * kPitchY + 5);
            found += HitTester::topmostAt(*m_screen, p, {.tolerance = 3.0}) ? 1 : 0;
        }
        const double hit = double(timer.nsecsElapsed()) / 1e6 / kHits;
        qInfo("select all %.1f ms, frame with 2000 frames drawn %.0f ms, hit test %.3f ms", select,
              selectedFrame, hit);
        QVERIFY(found > 0);
        QVERIFY(hit < 5.0);
        m_canvas->setSelection({});
    }
};

QTEST_MAIN(LargeScreenTest)
#include "LargeScreenTest.moc"
