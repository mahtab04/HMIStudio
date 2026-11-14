#include "core/objects/Line.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/ObjectContract.h"

#include <QTest>

using namespace hmi;

class LineTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        Line line;
        test::checkDefaults(line);
        QCOMPARE(line.typeId(), Line::kTypeId);
        QCOMPARE(line.geometry().size(), QSizeF(100, 0));
        QCOMPARE(line.value<EnumValue>(Prop::LineDirection), EnumValue{Line::Falling});
    }

    void clone()
    {
        Line line;
        test::checkClone(line);
    }

    void rejections()
    {
        Line line;
        test::checkRejections(line);
        QCOMPARE(line.setProperty(Prop::LineDirection, EnumValue{7}).error().code,
                 ErrorCode::OutOfRange);
        QVERIFY(!line.setProperty(Prop::FillColor, QColor(Qt::red)));
    }

    void registered() { test::checkRegistered(Line::kTypeId, u"line"); }

    void directionChoosesDiagonal()
    {
        Line line;
        QVERIFY(line.setProperty(Prop::Height, 50.0));
        QCOMPARE(line.localLine(), QLineF(0, 0, 100, 50));
        QVERIFY(line.setProperty(Prop::LineDirection, EnumValue{Line::Rising}));
        QCOMPARE(line.localLine(), QLineF(0, 50, 100, 0));
    }

    void hitShapeFollowsTheStroke()
    {
        Line line;
        QVERIFY(line.setProperty(Prop::Height, 100.0));
        QVERIFY(line.setProperty(Prop::LineWidth, 4.0));
        const QPainterPath shape = line.hitShape();
        QVERIFY(shape.contains(QPointF(50, 50)));
        QVERIFY(shape.contains(QPointF(50, 51)));
        // Empty corners of the bounding box are not on the line.
        QVERIFY(!shape.contains(QPointF(90, 10)));
        QVERIFY(!shape.contains(QPointF(10, 90)));
    }

    void hairlineIsStillHittable()
    {
        Line line;
        QVERIFY(line.setProperty(Prop::LineWidth, 0.0));
        QVERIFY(line.hitShape().contains(QPointF(50, 0.2)));
    }

    void paintsAlongTheDiagonal()
    {
        Line line;
        QVERIFY(line.setProperty(Prop::Height, 100.0));
        QVERIFY(line.setProperty(Prop::LineWidth, 6.0));
        QVERIFY(line.setProperty(Prop::LineColor, QColor(Qt::red)));
        const QImage image = test::render(line);
        QVERIFY(test::sameColour(image.pixel(50, 50), Qt::red));
        QCOMPARE(qAlpha(image.pixel(90, 10)), 0);
    }

    void visitorDispatchesToLine()
    {
        struct Counter : ObjectVisitor
        {
            using ObjectVisitor::visit;
            int lines = 0;
            void visit(Line&) override { ++lines; }
        } counter;
        Line line;
        line.accept(counter);
        QCOMPARE(counter.lines, 1);
    }
};

QTEST_MAIN(LineTest)
#include "LineTest.moc"
