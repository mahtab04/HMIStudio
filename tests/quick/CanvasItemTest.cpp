#include "quick/CanvasItem.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "quick/ObjectItem.h"
#include "quick/ScreenItem.h"

#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

#include <memory>

using namespace hmi;

class CanvasItemTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    CanvasItem* m_canvas = nullptr;
    ObjectId m_rect;
    ObjectId m_ellipse;

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(QStringLiteral("S"));
        builder.size({800, 600});
        builder.add(Rectangle::kTypeId, {10, 20, 100, 50});
        m_rect = builder.lastId();
        builder.add(Ellipse::kTypeId, {200, 200, 80, 80});
        m_ellipse = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());

        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(848, 648);
        m_canvas = new CanvasItem(m_window->contentItem());
        m_canvas->setSize(QSizeF(848, 648));
        m_canvas->setScreen(m_screen, &m_project->notifier());
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup()
    {
        m_window.reset();
        m_project.reset();
    }

    void fitsScreenOnFirstLayout()
    {
        // 800 x 600 screen in 848 x 648 with a 24 px margin: exactly 100 %.
        QCOMPARE(m_canvas->zoom(), 1.0);
        QCOMPARE(m_canvas->panOffset(), QPointF(24, 24));
        QCOMPARE(m_canvas->mapSceneToView({0, 0}), QPointF(24, 24));
    }

    void createsOneItemPerObject()
    {
        ScreenItem* screenItem = m_canvas->screenItem();
        QCOMPARE(screenItem->objectItemCount(), 2);
        ObjectItem* rect = screenItem->objectItem(m_rect);
        QVERIFY(rect);
        QCOMPARE(rect->position(), QPointF(10, 20));
        QCOMPARE(rect->size(), QSizeF(100, 50));
        QVERIFY(screenItem->objectItem(m_ellipse)->z() > rect->z());
    }

    void followsDocumentChanges()
    {
        ScreenItem* screenItem = m_canvas->screenItem();
        HmiObject* rect = m_screen->object(m_rect);
        QVERIFY(rect->setProperty(Prop::X, 300.0));
        QVERIFY(rect->setProperty(Prop::Rotation, 45.0));
        QCOMPARE(screenItem->objectItem(m_rect)->x(), 300.0);
        QCOMPARE(screenItem->objectItem(m_rect)->rotation(), 45.0);

        QVERIFY(m_screen->moveObject(m_rect, 1));
        QVERIFY(screenItem->objectItem(m_rect)->z() > screenItem->objectItem(m_ellipse)->z());

        auto added = m_screen->insertObject(std::make_unique<Ellipse>());
        QVERIFY(added);
        QCOMPARE(screenItem->objectItemCount(), 3);

        auto taken = m_screen->takeObject(m_ellipse);
        QVERIFY(taken);
        QCOMPARE(screenItem->objectItemCount(), 2);
        QVERIFY(screenItem->objectItem(m_ellipse) == nullptr);
    }

    void zoomKeepsPointUnderCursor()
    {
        const QPointF view(300, 200);
        const QPointF scene = m_canvas->mapViewToScene(view);
        m_canvas->zoomAt(view, 2.5);
        QCOMPARE(m_canvas->zoom(), 2.5);
        QCOMPARE(m_canvas->mapSceneToView(scene), view);
    }

    void zoomIsClamped()
    {
        m_canvas->zoomAt({0, 0}, 1000.0);
        QCOMPARE(m_canvas->zoom(), CanvasItem::kMaxZoom);
        m_canvas->zoomAt({0, 0}, 1e-6);
        QCOMPARE(m_canvas->zoom(), CanvasItem::kMinZoom);
    }

    void textureFollowsZoomWhenSettled()
    {
        ObjectItem* rect = m_canvas->screenItem()->objectItem(m_rect);
        const double dpr = m_window->effectiveDevicePixelRatio();
        m_canvas->zoomAt({0, 0}, 4.0);
        // During the gesture the old texture is scaled by the GPU.
        QCOMPARE(rect->textureSize(), QSize(qCeil(100 * dpr), qCeil(50 * dpr)));
        QSignalSpy settled(m_canvas, &CanvasItem::zoomSettled);
        QVERIFY(settled.wait());
        QCOMPARE(rect->textureSize(), QSize(qCeil(400 * dpr), qCeil(200 * dpr)));
    }

    void textureSizeIsCapped()
    {
        HmiObject* rect = m_screen->object(m_rect);
        QVERIFY(rect->setProperty(Prop::Width, 3000.0));
        m_canvas->zoomAt({0, 0}, 4.0);
        QSignalSpy settled(m_canvas, &CanvasItem::zoomSettled);
        QVERIFY(settled.wait());
        QCOMPARE(m_canvas->screenItem()->objectItem(m_rect)->textureSize().width(),
                 ObjectItem::kMaxTextureSide);
    }

    void previewMovesItemWithoutTouchingModel()
    {
        ObjectItem* item = m_canvas->screenItem()->objectItem(m_rect);
        m_canvas->setPreviewGeometry(m_rect, QRectF(50, 60, 200, 100));
        QCOMPARE(item->position(), QPointF(50, 60));
        QCOMPARE(item->size(), QSizeF(200, 100));
        QCOMPARE(m_screen->object(m_rect)->geometry(), QRectF(10, 20, 100, 50));

        m_canvas->clearPreview();
        QCOMPARE(item->position(), QPointF(10, 20));
        QCOMPARE(item->size(), QSizeF(100, 50));
    }

    void rendersSelectionHandles()
    {
        m_canvas->setHandleFillColor(Qt::red);
        m_canvas->setSelection({m_rect});
        // Rect (10, 20, 100, 50) at 100 % with a 24 px margin: bottom-right
        // handle centred on view (134, 94).
        QImage image = m_window->grabWindow();
        const qreal dpr = image.devicePixelRatio();
        const auto pixel = [&](int x, int y)
        {
            return image.pixelColor(int(x * dpr), int(y * dpr));
        };
        QCOMPARE(pixel(134, 94), QColor(Qt::red));

        m_canvas->setSelection({});
        image = m_window->grabWindow();
        QVERIFY(pixel(134, 94) != QColor(Qt::red));
    }

    void overlayFollowsCanvasPosition()
    {
        // Selection changes after the window is shown, with the canvas away
        // from the window origin.
        m_canvas->setPosition(QPointF(100, 50));
        m_canvas->setSize(QSizeF(848, 648));
        m_canvas->fitScreen();
        m_canvas->setHandleFillColor(Qt::red);
        QImage image = m_window->grabWindow();
        m_canvas->setSelection({m_rect});
        image = m_window->grabWindow();
        const qreal dpr = image.devicePixelRatio();
        const QPointF handle = m_canvas->mapToScene(m_canvas->mapSceneToView({110, 70}));
        QCOMPARE(image.pixelColor(int(handle.x() * dpr), int(handle.y() * dpr)), QColor(Qt::red));
    }

    void rendersRubberBand()
    {
        m_canvas->setRubberBandColor(QColor(0, 255, 0));
        m_canvas->setRubberBand(QRectF(300, 300, 100, 100)); // empty screen area
        const QImage image = m_window->grabWindow();
        const qreal dpr = image.devicePixelRatio();
        QCOMPARE(image.pixelColor(int(374 * dpr), int(374 * dpr)), QColor(0, 255, 0));
    }

    void screenSwapClearsItems()
    {
        m_canvas->setScreen(nullptr, nullptr);
        QCOMPARE(m_canvas->screenItem()->objectItemCount(), 0);
    }
};

QTEST_MAIN(CanvasItemTest)
#include "CanvasItemTest.moc"
