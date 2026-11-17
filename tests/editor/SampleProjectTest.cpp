#include "editor/app/SampleProject.h"

#include "core/io/ProjectSerializer.h"
#include "core/property/Prop.h"
#include "editor/app/FillingStationDriver.h"

#include <QTemporaryDir>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

class SampleProjectTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void exampleProjectMatchesTheSample()
    {
        // examples/BufferFilling.hmiproj is the built-in sample saved: it
        // must load, and its tags must be the sample's.
        auto example = ProjectSerializer::load(QString::fromUtf8(HMI_EXAMPLE_PROJECT));
        QVERIFY2(example, example ? "" : qPrintable(example.error().message));
        const auto sample = makeSampleProject();
        QCOMPARE((*example)->tags().size(), sample->tags().size());
        for (const Tag& tag : sample->tags().tags())
        {
            const Tag* saved = (*example)->tags().find(tag.name);
            QVERIFY2(saved, qPrintable(tag.name));
            QCOMPARE(saved->type, tag.type);
            QCOMPARE(saved->limitInput, tag.limitInput);
        }
        QVERIFY((*example)->tags().find(u"FlowSetpoint"_s)->limitInput);
    }

    void processRespondsToCommands()
    {
        auto project = makeSampleProject();
        FillingStationDriver driver;
        QVERIFY(driver.start(project->tags().tags()));
        QHash<QString, TagValue> values;
        const auto collect = [&](const QString& name, const TagValue& value)
        {
            values.insert(name.toLower(), value);
        };
        driver.poll(0, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(false));
        QVERIFY(driver.write(u"RunCommand"_s, true));
        for (int i = 1; i <= 50; ++i)
            driver.poll(i * 0.1, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(true));
        QVERIFY(TagValues::toNumber(values.value(u"flow"_s)).value() > 49);
        QVERIFY(TagValues::toNumber(values.value(u"level"_s)).value() > 35);
        QVERIFY(TagValues::toNumber(values.value(u"pressure"_s)).value() > 2);
        QVERIFY(driver.write(u"FlowSetpoint"_s, 75.0));
        for (int i = 51; i <= 80; ++i)
            driver.poll(i * 0.1, collect);
        QVERIFY(TagValues::toNumber(values.value(u"flow"_s)).value() > 74);
        QVERIFY(driver.write(u"RunCommand"_s, false));
        driver.poll(8.1, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(false));
        QVERIFY(driver.write(u"Jog"_s, true));
        driver.poll(8.2, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(true));
        QVERIFY(driver.write(u"Jog"_s, false));
        driver.poll(8.3, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(false));
        driver.stop();
        QVERIFY(driver.start(project->tags().tags()));
        driver.poll(0, collect);
        QCOMPARE(values.value(u"runcommand"_s), TagValue(false));
        QCOMPARE(values.value(u"jog"_s), TagValue(false));
    }
    void autoBandAndHighLevelInterlock()
    {
        auto project = makeSampleProject();
        std::vector<Tag> tags(project->tags().tags().begin(), project->tags().tags().end());
        for (auto& tag : tags)
            if (tag.name == u"Level")
                tag.initial = 81.0;
        FillingStationDriver driver;
        QVERIFY(driver.start(tags));
        QVERIFY(driver.write(u"RunCommand"_s, true));
        QHash<QString, TagValue> values;
        const auto collect = [&](const QString& name, const TagValue& value)
        {
            values.insert(name.toLower(), value);
        };
        driver.poll(0, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(false));
        for (int i = 1; i <= 100; ++i)
            driver.poll(i * 0.5, collect);
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(true)); // demand drained below 30%
        driver.stop();
        for (auto& tag : tags)
            if (tag.name == u"Level")
                tag.initial = 95.0;
        QVERIFY(driver.start(tags));
        QVERIFY(driver.write(u"RunCommand"_s, true));
        QVERIFY(driver.write(u"Jog"_s, true));
        QVERIFY(driver.write(u"AutoMode"_s, false));
        driver.poll(0, collect);
        QCOMPARE(values.value(u"highlevel"_s), TagValue(true));
        QCOMPARE(values.value(u"pumprunning"_s), TagValue(false));
    }
    void sampleSurvivesSavingAndExports()
    {
        auto project = makeSampleProject();
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(*project, dir.path()));
        auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(loaded);
        QCOMPARE((*loaded)->simulationProfile(), u"filling-station"_s);
        QCOMPARE((*loaded)->tags().size(), std::size_t(10));
        int actions = 0;
        for (const auto& object : (*loaded)->screens()[0]->objects())
        {
            if (object->runtimeBehavior().kind == RuntimeBehavior::Kind::Action)
            {
                QVERIFY((*loaded)->tags().find(object->value<TagRef>(Prop::ActionTag).tagName));
                ++actions;
            }
        }
        QCOMPARE(actions, 6);
        if (const QString folder = qEnvironmentVariable("HMI_SAMPLE_EXPORT"); !folder.isEmpty())
            QVERIFY(ProjectSerializer::save(*project, folder));
    }
};
QTEST_MAIN(SampleProjectTest)
#include "SampleProjectTest.moc"
