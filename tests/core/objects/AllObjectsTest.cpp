// The object contract (ObjectContract.h) for every registered toolbox type,
// so a new type is checked as soon as it registers: defaults, clone,
// rejections, painting, hit shape, and a JSON round trip.

#include "core/io/ObjectJson.h"
#include "core/model/HmiObject.h"
#include "core/objects/ObjectContract.h"
#include "core/registry/ObjectRegistry.h"

#include <QPainter>
#include <QTest>

using namespace hmi;

Q_DECLARE_METATYPE(hmi::ObjectTypeId)

class AllObjectsTest : public QObject
{
    Q_OBJECT

private:
    static void addTypeRows()
    {
        QTest::addColumn<ObjectTypeId>("type");
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            if (info.inToolbox)
                QTest::newRow(qPrintable(info.key)) << info.id;
        }
    }

private Q_SLOTS:
    void contract_data() { addTypeRows(); }
    void contract()
    {
        QFETCH(ObjectTypeId, type);
        const ObjectTypeInfo* info = ObjectRegistry::instance().find(type);
        test::checkRegistered(type, info->key);
        auto object = ObjectRegistry::instance().create(type);
        test::checkDefaults(*object);
        test::checkRejections(*object);
        test::checkClone(*object);
        QVERIFY(info->displayName && *info->displayName);
        QVERIFY(info->iconPath.startsWith(u":/icons/"));
    }

    // Every type draws something at its default size. (Callers save and
    // restore the painter around paint(), so a type may leave it changed.)
    void paints_data() { addTypeRows(); }
    void paints()
    {
        QFETCH(ObjectTypeId, type);
        const auto object = ObjectRegistry::instance().create(type);
        const QSize size = object->geometry().size().toSize().expandedTo(QSize(1, 1));
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        object->paint(painter, PaintContext{.zoom = 1.0});
        painter.end();

        bool drawn = false;
        for (int y = 0; y < image.height() && !drawn; ++y)
        {
            for (int x = 0; x < image.width() && !drawn; ++x)
                drawn = qAlpha(image.pixel(x, y)) > 0;
        }
        QVERIFY2(drawn, "nothing was painted");

        // Painting at low detail and at a deep zoom must not fail either.
        QPainter again(&image);
        object->paint(again, PaintContext{.zoom = 0.2});
        object->paint(again, PaintContext{.zoom = 8.0});
    }

    // The hit shape lies within the bounds (plus a stroke) and is not empty.
    void hitShapeFitsBounds_data() { addTypeRows(); }
    void hitShapeFitsBounds()
    {
        QFETCH(ObjectTypeId, type);
        const auto object = ObjectRegistry::instance().create(type);
        const QPainterPath shape = object->hitShape();
        QVERIFY(!shape.isEmpty());
        const QRectF bounds(QPointF(0, 0), object->geometry().size());
        QVERIFY(bounds.adjusted(-10, -10, 10, 10).contains(shape.boundingRect()));
    }

    void jsonRoundTrip_data() { addTypeRows(); }
    void jsonRoundTrip()
    {
        QFETCH(ObjectTypeId, type);
        auto object = ObjectRegistry::instance().create(type);
        QVERIFY(object->setProperty(Prop::Name, QStringLiteral("saved")));
        auto back = ObjectJson::read(ObjectJson::write(*object));
        QVERIFY(back);
        QCOMPARE((*back)->id(), object->id());
        QVERIFY((*back)->hasSameProperties(*object));
    }
};

QTEST_MAIN(AllObjectsTest)
#include "AllObjectsTest.moc"
