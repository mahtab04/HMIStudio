#include "core/objects/Rectangle.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/ObjectContract.h"

#include <QTest>

using namespace hmi;

class RectangleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        Rectangle rect;
        test::checkDefaults(rect);
        QCOMPARE(rect.typeId(), Rectangle::kTypeId);
        QCOMPARE(rect.geometry(), QRectF(0, 0, 100, 60));
        QCOMPARE(rect.value<double>(Prop::CornerRadius), 0.0);
    }

    void clone()
    {
        Rectangle rect;
        QVERIFY(rect.setProperty(Prop::FillColor, QColor(Qt::green)));
        test::checkClone(rect);
        QCOMPARE(rect.clone()->value<QColor>(Prop::FillColor), QColor(Qt::green));
    }

    void rejections()
    {
        Rectangle rect;
        test::checkRejections(rect);
        QVERIFY(!rect.setProperty(Prop::CornerRadius, -2.0));
        QVERIFY(!rect.setProperty(Prop::LineWidth, 51.0));
        QVERIFY(!rect.setProperty(Prop::FillColor, 3.0));
    }

    void registered() { test::checkRegistered(Rectangle::kTypeId, u"rectangle"); }

    void hitShapeIsWholeRectangle()
    {
        Rectangle rect;
        const QPainterPath shape = rect.hitShape();
        QVERIFY(shape.contains(QPointF(1, 1)));
        QVERIFY(shape.contains(QPointF(99, 59)));
        QVERIFY(!shape.contains(QPointF(101, 30)));
    }

    void roundedCornersAreNotHit()
    {
        Rectangle rect;
        QVERIFY(rect.setProperty(Prop::CornerRadius, 20.0));
        QVERIFY(!rect.hitShape().contains(QPointF(1, 1)));
        QVERIFY(rect.hitShape().contains(QPointF(50, 30)));
    }

    void paintsFillAndOutline()
    {
        Rectangle rect;
        QVERIFY(rect.setProperty(Prop::FillColor, QColor(Qt::red)));
        QVERIFY(rect.setProperty(Prop::LineColor, QColor(Qt::blue)));
        QVERIFY(rect.setProperty(Prop::LineWidth, 4.0));
        const QImage image = test::render(rect);
        QVERIFY(test::sameColour(image.pixel(50, 30), Qt::red));
        QVERIFY(test::sameColour(image.pixel(1, 30), Qt::blue));
    }

    void visitorDispatchesToRectangle()
    {
        struct Counter : ObjectVisitor
        {
            using ObjectVisitor::visit;
            int rectangles = 0;
            void visit(Rectangle&) override { ++rectangles; }
        } counter;
        Rectangle rect;
        rect.accept(counter);
        QCOMPARE(counter.rectangles, 1);
    }
};

QTEST_MAIN(RectangleTest)
#include "RectangleTest.moc"
