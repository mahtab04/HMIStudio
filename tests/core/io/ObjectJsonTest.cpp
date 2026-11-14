#include "core/io/ObjectJson.h"

#include "core/model/GroupObject.h"
#include "core/model/HmiObject.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"
#include "core/property/Prop.h"
#include "core/registry/ObjectRegistry.h"

#include <QJsonDocument>
#include <QTest>

using namespace Qt::StringLiterals;

using namespace hmi;

class ObjectJsonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyTypeRoundTrips()
    {
        for (const ObjectTypeInfo& info : ObjectRegistry::instance().types())
        {
            const auto object = info.create(ObjectId::create());
            const QJsonObject json = ObjectJson::write(*object);
            QCOMPARE(json.value(u"type").toString(), info.key);
            auto back = ObjectJson::read(json);
            QVERIFY2(back.has_value(), qPrintable(info.key));
            QCOMPARE((*back)->id(), object->id());
            QVERIFY2((*back)->hasSameProperties(*object), qPrintable(info.key));
        }
    }

    void valuesUseReadableEncodings()
    {
        TextBox text;
        QVERIFY(text.setProperty(Prop::FillColor, QColor(0x11, 0x22, 0x33, 0x44)));
        QVERIFY(text.setProperty(Prop::Text, QStringLiteral("Pump 1")));
        const QJsonObject props = ObjectJson::write(text).value(u"props").toObject();
        QCOMPARE(props.value(u"fillColor").toString(), QStringLiteral("#44112233"));
        QCOMPARE(props.value(u"text").toString(), QStringLiteral("Pump 1"));
        QCOMPARE(props.value(u"x").toDouble(), 0.0);

        Line line;
        QVERIFY(line.setProperty(Prop::LineDirection, EnumValue{Line::Rising}));
        const QJsonObject lineProps = ObjectJson::write(line).value(u"props").toObject();
        QCOMPARE(lineProps.value(u"lineDirection").toString(), QStringLiteral("rising"));
        auto back = ObjectJson::read(ObjectJson::write(line));
        QVERIFY(back);
        QVERIFY((*back)->hasSameProperties(line));
    }

    void groupKeepsChildren()
    {
        std::vector<std::unique_ptr<HmiObject>> objects;
        objects.push_back(std::make_unique<Rectangle>());
        auto second = std::make_unique<Rectangle>();
        QVERIFY(second->setProperty(Prop::X, 200.0));
        objects.push_back(std::move(second));
        const auto group = GroupObject::create(std::move(objects));

        // Through text, as the clipboard carries it.
        const QByteArray bytes = QJsonDocument(ObjectJson::write(*group)).toJson();
        auto back = ObjectJson::read(QJsonDocument::fromJson(bytes).object());
        QVERIFY(back);
        QCOMPARE((*back)->children().size(), std::size_t(2));
        QVERIFY((*back)->hasSameProperties(*group));
        QCOMPARE((*back)->children()[1]->id(), group->children()[1]->id());
    }

    void rejectsBadInput()
    {
        QCOMPARE(ObjectJson::read(QJsonObject{{u"type"_s, u"gauge"_s}}).error().code,
                 ErrorCode::UnknownType);

        QJsonObject json = ObjectJson::write(Rectangle());
        QJsonObject props = json.value(u"props").toObject();
        props.insert(u"width"_s, u"wide"_s);
        json.insert(u"props"_s, props);
        QCOMPARE(ObjectJson::read(json).error().code, ErrorCode::TypeMismatch);

        props.insert(u"width"_s, -5);
        json.insert(u"props"_s, props);
        QCOMPARE(ObjectJson::read(json).error().code, ErrorCode::OutOfRange);

        // Children on a type that cannot have them.
        json = ObjectJson::write(Rectangle());
        json.insert(u"children"_s, QJsonArray{ObjectJson::write(Rectangle())});
        QVERIFY(!ObjectJson::read(json));
    }

    void ignoresUnknownProperties()
    {
        QJsonObject json = ObjectJson::write(Rectangle());
        QJsonObject props = json.value(u"props").toObject();
        props.insert(u"futureThing"_s, 42);
        json.insert(u"props"_s, props);
        QVERIFY(ObjectJson::read(json));
    }

    void missingIdGetsFreshOne()
    {
        QJsonObject json = ObjectJson::write(Rectangle());
        json.remove(u"id"_s);
        auto object = ObjectJson::read(json);
        QVERIFY(object);
        QVERIFY(!(*object)->id().isNull());
    }
};

QTEST_MAIN(ObjectJsonTest)
#include "ObjectJsonTest.moc"
