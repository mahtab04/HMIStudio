// Simulated tags and play mode: the simulator's values, the TagEngine's
// worker thread and coalescing, and the BindingEvaluator driving a play
// copy of a screen while the document stays untouched.

#include "comm/BindingEvaluator.h"
#include "comm/SimulatorDriver.h"
#include "comm/TagEngine.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Pump.h"
#include "core/objects/Tank.h"
#include "core/property/Prop.h"

#include <QSignalSpy>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

Tag makeTag(const QString& name, Tag::Simulation simulation, Tag::Type type = Tag::Type::Real)
{
    Tag tag;
    tag.name = name;
    tag.type = type;
    tag.minimum = 0;
    tag.maximum = 100;
    tag.period = 10;
    tag.simulation = simulation;
    return tag;
}

double number(const TagValue& value)
{
    return TagValues::toNumber(value).value_or(-1.0);
}

} // namespace

class SimulationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void simulationModes()
    {
        const auto at = [](Tag::Simulation mode, double seconds, Tag::Type type = Tag::Type::Real)
        {
            return SimulatorDriver::simulate(makeTag(u"T"_s, mode, type), seconds, 0.25);
        };
        QCOMPARE(number(at(Tag::Simulation::Ramp, 2.5)), 25.0);
        QCOMPARE(number(at(Tag::Simulation::Ramp, 12.5)), 25.0); // repeats
        QCOMPARE(number(at(Tag::Simulation::Sine, 0.0)), 50.0);
        QCOMPARE(number(at(Tag::Simulation::Sine, 2.5)), 100.0); // a quarter period
        QCOMPARE(number(at(Tag::Simulation::Random, 3.0)), 25.0);
        QCOMPARE(at(Tag::Simulation::Toggle, 2.0, Tag::Type::Bool), TagValue(false));
        QCOMPARE(at(Tag::Simulation::Toggle, 7.0, Tag::Type::Bool), TagValue(true));
        QCOMPARE(number(at(Tag::Simulation::Toggle, 7.0)), 100.0); // numbers toggle min/max
    }

    void driverPublishesChangesOnly()
    {
        SimulatorDriver driver(42);
        Tag fixed = makeTag(u"Setpoint"_s, Tag::Simulation::None);
        fixed.initial = TagValue(30.0);
        const std::vector<Tag> tags{makeTag(u"Ramp"_s, Tag::Simulation::Ramp), fixed};
        QVERIFY(driver.start(tags));
        QHash<QString, TagValue> seen;
        const auto collect = [&seen](const QString& t, const TagValue& v)
        {
            seen.insert(t, v);
        };
        driver.poll(1.0, collect);
        QCOMPARE(seen.size(), 2);
        QCOMPARE(number(seen.value(u"Setpoint"_s)), 30.0);
        seen.clear();
        driver.poll(2.0, collect);
        QCOMPARE(seen.keys(), QStringList{u"Ramp"_s}); // the fixed tag did not change
        QVERIFY(driver.write(u"setpoint"_s, TagValue(55.0)));
        driver.poll(2.0, collect);
        QCOMPARE(number(seen.value(u"Setpoint"_s)), 55.0);
        QVERIFY(!driver.write(u"nope"_s, TagValue(1.0)));
    }

    void engineRunsOnAWorkerAndCoalesces()
    {
        TagEngine engine;
        QSignalSpy changed(&engine, &TagEngine::valuesChanged);
        Tag fixed = makeTag(u"Setpoint"_s, Tag::Simulation::None);
        fixed.initial = TagValue(30.0);
        const std::vector<Tag> tags{makeTag(u"Level"_s, Tag::Simulation::Ramp), fixed};
        engine.start(tags);
        QVERIFY(engine.isRunning());
        // Start values are there at once.
        QCOMPARE(changed.count(), 1);
        QCOMPARE(number(engine.value(u"setpoint"_s)), 30.0);
        QCOMPARE(number(engine.value(u"Level"_s)), 0.0);

        // The ramp moves; updates come at most every kPublishMs.
        QTRY_VERIFY_WITH_TIMEOUT(number(engine.value(u"Level"_s)) > 0.0, 2000);
        QElapsedTimer timer;
        timer.start();
        changed.clear();
        QTest::qWait(600);
        QVERIFY(changed.count() >= 3);
        QVERIFY(changed.count() <= 600 / TagEngine::kPublishMs + 2);
        for (const QList<QVariant>& call : std::as_const(changed))
            QCOMPARE(call.front().toStringList(), QStringList{u"Level"_s});

        engine.write(u"Setpoint"_s, TagValue(u"75"_s)); // converted to the tag's type
        QCOMPARE(engine.value(u"Setpoint"_s), TagValue(75.0));
        engine.write(u"Unknown"_s, TagValue(1.0)); // ignored

        engine.stop();
        QVERIFY(!engine.isRunning());
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Level"_s)));

        // Passing a simulator explicitly preserves the writable driver API.
        engine.start(tags, std::make_unique<SimulatorDriver>());
        QVERIFY(engine.isSimulating());
        QVERIFY(!engine.isReadOnly());
        engine.write(u"Setpoint"_s, TagValue(17.0));
        QCOMPARE(engine.value(u"Setpoint"_s), TagValue(17.0));
        engine.stop();
    }

    void evaluatorDrivesAPlayCopy()
    {
        Project project;
        auto builder = ScreenBuilder(u"Filling"_s);
        builder.add(Tank::kTypeId, {0, 0, 110, 160});
        const ObjectId tankId = builder.lastId();
        builder.add(Pump::kTypeId, {200, 0, 80, 80});
        const ObjectId pumpId = builder.lastId();
        Screen* screen = *project.insertScreen(builder.build());
        QVERIFY(screen->object(tankId)->setBinding({Prop::Value, u"Level"_s, nullptr}));
        QVERIFY(screen->object(pumpId)->setBinding({Prop::Running, u"Pump"_s, nullptr}));
        // Text cannot drive a number: reported, not applied.
        QVERIFY(screen->object(tankId)->setBinding({Prop::Width, u"Label"_s, nullptr}));

        Tag level = makeTag(u"Level"_s, Tag::Simulation::None);
        level.initial = TagValue(42.0);
        Tag pump = makeTag(u"Pump"_s, Tag::Simulation::None, Tag::Type::Bool);
        pump.initial = TagValue(true);
        Tag label = makeTag(u"Label"_s, Tag::Simulation::None, Tag::Type::String);
        label.initial = TagValue(u"high"_s);
        const std::vector<Tag> tags{level, pump, label};

        TagEngine engine;
        engine.start(tags);
        BindingEvaluator evaluator(engine);
        evaluator.show(*screen);
        const Screen* play = evaluator.playScreen();
        QVERIFY(play && play != screen);
        QCOMPARE(play->object(tankId)->value<double>(Prop::Value), 42.0);
        QVERIFY(play->object(pumpId)->value<bool>(Prop::Running));
        QCOMPARE(evaluator.problems().size(), 1);
        QVERIFY(evaluator.problems().front().contains(u"width"));

        // New values reach the copy through its own notifier.
        QSignalSpy changed(&evaluator.playProject().notifier(), &DocumentNotifier::propertyChanged);
        engine.write(u"Level"_s, TagValue(80.0));
        QTRY_COMPARE(play->object(tankId)->value<double>(Prop::Value), 80.0);
        QVERIFY(!changed.isEmpty());

        // The document never changes.
        QCOMPARE(screen->object(tankId)->value<double>(Prop::Value), 60.0);
        QVERIFY(!screen->object(pumpId)->value<bool>(Prop::Running));
        evaluator.clear();
        QVERIFY(!evaluator.playScreen());
    }
};

QTEST_MAIN(SimulationTest)
#include "SimulationTest.moc"
