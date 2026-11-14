#include "core/objects/Ellipse.h"

#include "core/model/ObjectVisitor.h"
#include "core/objects/ObjectContract.h"

#include <QTest>

using namespace hmi;

class EllipseTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaults()
    {
        Ellipse ellipse;
        test::checkDefaults(ellipse);
        QCOMPARE(ellipse.typeId(), Ellipse::kTypeId);
        QCOMPARE(ellipse.geometry().size(), QSizeF(80, 80));
        QVERIFY(!ellipse.property(Prop::CornerRadius).has_value());
    }

    void clone()
    {
        Ellipse ellipse;
        test::checkClone(ellipse);
    }

    void rejections()
    {
        Ellipse ellipse;
        test::checkRejections(ellipse);
        QCOMPARE(ellipse.setProperty(Prop::CornerRadius, 4.0).error().code,
                 ErrorCode::UnknownProperty);
    }

    void registered() { test::checkRegistered(Ellipse::kTypeId, u"ellipse"); }

    void hitShapeExcludesCorners()
    {
        Ellipse ellipse;
        const QPainterPath shape = ellipse.hitShape();
        QVERIFY(shape.contains(QPointF(40, 40)));
        QVERIFY(shape.contains(QPointF(2, 40)));
        QVERIFY(!shape.contains(QPointF(3, 3)));
        QVERIFY(!shape.contains(QPointF(77, 77)));
    }

    void paintsInsideOnly()
    {
        Ellipse ellipse;
        QVERIFY(ellipse.setProperty(Prop::FillColor, QColor(Qt::red)));
        const QImage image = test::render(ellipse);
        QVERIFY(test::sameColour(image.pixel(40, 40), Qt::red));
        QCOMPARE(qAlpha(image.pixel(1, 1)), 0);
    }

    void visitorDispatchesToEllipse()
    {
        struct Counter : ObjectVisitor
        {
            using ObjectVisitor::visit;
            int ellipses = 0;
            void visit(Ellipse&) override { ++ellipses; }
        } counter;
        Ellipse ellipse;
        ellipse.accept(counter);
        QCOMPARE(counter.ellipses, 1);
    }
};

QTEST_MAIN(EllipseTest)
#include "EllipseTest.moc"
