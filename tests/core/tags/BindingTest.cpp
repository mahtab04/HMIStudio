#include "core/tags/Binding.h"

#include "core/io/ObjectJson.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Rectangle.h"
#include "core/objects/Tank.h"
#include "core/property/Prop.h"
#include "core/tags/Expression.h"
#include "core/tags/TagUsage.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

const PropertyDescriptor& descriptor(const HmiObject& object, PropertyId id)
{
    return *findDescriptor(object.descriptors(), id);
}

TransformPtr make(const QString& kind, const QJsonObject& parameters)
{
    auto t = TransformRegistry::instance().create(kind, parameters);
    Q_ASSERT(t);
    return *t;
}

} // namespace

class BindingTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void expressions_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("x");
        QTest::addColumn<double>("expected");
        QTest::newRow("scale") << u"x * 1.8 + 32"_s << 100.0 << 212.0;
        QTest::newRow("precedence") << u"2 + 3 * x"_s << 2.0 << 8.0;
        QTest::newRow("parentheses") << u"(2 + 3) * x"_s << 2.0 << 10.0;
        QTest::newRow("unary") << u"-x + --2"_s << 1.0 << 1.0;
        QTest::newRow("choose") << u"x > 80 ? 1 : 0"_s << 90.0 << 1.0;
        QTest::newRow("logic") << u"x >= 10 && x <= 20 || !true"_s << 15.0 << 1.0;
        QTest::newRow("not equal") << u"x != 3"_s << 3.0 << 0.0;
        QTest::newRow("functions") << u"clamp(round(x / 10) * 10, 0, 50)"_s << 74.0 << 50.0;
        QTest::newRow("min max") << u"max(x, 2, 9) - min(4, x)"_s << 3.0 << 6.0;
        QTest::newRow("value") << u"pow(value, 2) % 7"_s << 4.0 << 2.0;
        QTest::newRow("exponent") << u"1.5e2 + x"_s << 0.0 << 150.0;
    }
    void expressions()
    {
        QFETCH(QString, text);
        QFETCH(double, x);
        QFETCH(double, expected);
        auto e = Expression::parse(text);
        QVERIFY2(e, qPrintable(e ? QString() : e.error().message));
        QCOMPARE(e->evaluate(x), expected);
    }

    void badExpressionsSayWhere()
    {
        for (const QString& text :
             {u"x +"_s, u"foo(x)"_s, u"(x"_s, u"abs(1, 2)"_s, u"x ? 1"_s, u"x $ 2"_s, u""_s})
        {
            auto e = Expression::parse(text);
            QVERIFY2(!e, qPrintable(text));
            QVERIFY(e.error().message.contains(u"position"));
        }
    }

    void transformsConvertToTheProperty()
    {
        Tank tank;
        const PropertyDescriptor& level = descriptor(tank, Prop::Value);
        const PropertyDescriptor& fill = descriptor(tank, Prop::FillColor);
        const PropertyDescriptor& visible = descriptor(tank, Prop::Visible);

        QCOMPARE(*Binding::direct()->apply(TagValue(42.0), level), PropertyValue(42.0));
        QCOMPARE(*Binding::direct()->apply(TagValue(qint64(1)), visible), PropertyValue(true));
        QVERIFY(!Binding::direct()->apply(TagValue(u"high"_s), level));

        const auto linear =
            make(u"linear"_s,
                 {{u"inMin"_s, 0}, {u"inMax"_s, 4000}, {u"outMin"_s, 0}, {u"outMax"_s, 100}});
        QCOMPARE(*linear->apply(TagValue(qint64(1000)), level), PropertyValue(25.0));
        QCOMPARE(linear->summary(), u"0–4000 → 0–100"_s);

        const auto ranges =
            make(u"ranges"_s,
                 {{u"ranges"_s,
                   QJsonArray{
                       QJsonObject{{u"from"_s, 0}, {u"to"_s, 80}, {u"value"_s, u"#ff3b82f6"_s}},
                       QJsonObject{{u"from"_s, 80}, {u"to"_s, 1e9}, {u"value"_s, u"#ffe5533d"_s}}}},
                  {u"default"_s, u"#ff808080"_s}});
        QCOMPARE(*ranges->apply(TagValue(50.0), fill), PropertyValue(QColor(0x3b, 0x82, 0xf6)));
        QCOMPARE(*ranges->apply(TagValue(85.0), fill), PropertyValue(QColor(0xe5, 0x53, 0x3d)));
        QCOMPARE(*ranges->apply(TagValue(-5.0), fill), PropertyValue(QColor(0x80, 0x80, 0x80)));

        const auto expression = make(u"expression"_s, {{u"text"_s, u"x / 10"_s}});
        QCOMPARE(*expression->apply(TagValue(500.0), level), PropertyValue(50.0));
        const auto broken = make(u"expression"_s, {{u"text"_s, u"1 / (x - x)"_s}});
        QVERIFY(!broken->apply(TagValue(5.0), level));

        QVERIFY(!TransformRegistry::instance().create(u"magic"_s));
        QVERIFY(!TransformRegistry::instance().create(u"expression"_s, {{u"text"_s, u"x +"_s}}));
        // A value outside the property's limits is refused, not clamped.
        const PropertyDescriptor& width = descriptor(tank, Prop::Width);
        QVERIFY(!Binding::direct()->apply(TagValue(-10.0), width));
    }

    void objectsKeepTheirBindings()
    {
        Tank tank;
        QVERIFY(tank.setBinding({Prop::Value, u"Tank_Level"_s, nullptr}));
        QCOMPARE(tank.bindings().size(), std::size_t(1));
        QCOMPARE(tank.binding(Prop::Value)->transform->kind(), u"direct"_s);
        // Replacing, not adding a second one.
        QVERIFY(tank.setBinding({Prop::Value, u"Other"_s, make(u"linear"_s, {})}));
        QCOMPARE(tank.bindings().size(), std::size_t(1));
        QCOMPARE(tank.binding(Prop::Value)->tag, u"Other"_s);
        // Only bindable properties, and only with a tag.
        QVERIFY(!tank.setBinding({Prop::Name, u"T"_s, nullptr}));
        QVERIFY(!tank.setBinding({Prop::Value, u""_s, nullptr}));

        const auto copy = tank.clone();
        QVERIFY(copy->hasSameProperties(tank));
        QVERIFY(copy->removeBinding(Prop::Value));
        QVERIFY(!copy->hasSameProperties(tank));
        QVERIFY(!copy->removeBinding(Prop::Value));
    }

    void bindingsRoundTripThroughJson()
    {
        Tank tank;
        QVERIFY(tank.setBinding(
            {Prop::Value, u"Tank_Level"_s,
             make(u"linear"_s,
                  {{u"inMin"_s, 0}, {u"inMax"_s, 4000}, {u"outMin"_s, 0}, {u"outMax"_s, 100}})}));
        QVERIFY(tank.setBinding({Prop::FillColor, u"Alarm"_s,
                                 make(u"expression"_s, {{u"text"_s, u"x > 0 ? 1 : 0"_s}})}));
        QJsonObject json = ObjectJson::write(tank);
        QCOMPARE(json.value(u"bindings").toArray().size(), 2);
        QCOMPARE(json.value(u"bindings").toArray()[0].toObject().value(u"property").toString(),
                 u"value"_s);

        // A binding from a newer version (unknown transform) is kept as is.
        QJsonArray bindings = json.value(u"bindings").toArray();
        bindings.append(QJsonObject{{u"property"_s, u"value"_s},
                                    {u"tag"_s, u"X"_s},
                                    {u"transform"_s, QJsonObject{{u"kind"_s, u"fuzzy"_s}}}});
        json.insert(u"bindings"_s, bindings);
        auto back = ObjectJson::read(json);
        QVERIFY(back);
        QVERIFY((*back)->hasSameProperties(tank));
        QCOMPARE(ObjectJson::write(**back).value(u"bindings").toArray(), bindings);
    }

    void notifiesWhenInAProject()
    {
        Project project;
        Screen* screen = *project.insertScreen(
            ScreenBuilder(u"S"_s).add(Tank::kTypeId, {0, 0, 100, 160}).build());
        HmiObject& tank = *screen->objects().front();
        QSignalSpy changed(&project.notifier(), &DocumentNotifier::bindingChanged);
        QVERIFY(tank.setBinding({Prop::Value, u"L"_s, nullptr}));
        QVERIFY(tank.setBinding({Prop::Value, u"L"_s, nullptr})); // same: no signal
        QVERIFY(tank.removeBinding(Prop::Value));
        QCOMPARE(changed.count(), 2);
    }

    void whereUsed()
    {
        Project project;
        auto builder = ScreenBuilder(u"S"_s);
        builder.add(Tank::kTypeId, {0, 0, 100, 160});
        builder.add(Rectangle::kTypeId, {200, 0, 50, 50});
        Screen* screen = *project.insertScreen(builder.build());
        HmiObject& tank = *screen->objects()[0];
        HmiObject& rect = *screen->objects()[1];
        QVERIFY(tank.setBinding({Prop::Value, u"Level"_s, nullptr}));
        QVERIFY(tank.setBinding({Prop::FillColor, u"Alarm"_s, nullptr}));
        QVERIFY(rect.setBinding({Prop::Visible, u"level"_s, nullptr}));
        const auto uses = TagUsage::find(project, u"LEVEL"_s);
        QCOMPARE(uses.size(), std::size_t(2));
        QCOMPARE(uses[0], (TagUse{screen->id(), tank.id(), Prop::Value}));
        QCOMPARE(uses[1], (TagUse{screen->id(), rect.id(), Prop::Visible}));
        QVERIFY(TagUsage::find(project, u"Nothing"_s).empty());
    }
};

QTEST_MAIN(BindingTest)
#include "BindingTest.moc"
