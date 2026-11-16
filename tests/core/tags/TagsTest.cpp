#include "core/io/ProjectSerializer.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/tags/Tag.h"
#include "core/tags/TagCsv.h"
#include "core/tags/TagDatabase.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

Tag level()
{
    Tag tag;
    tag.name = u"Tank_Level"_s;
    tag.type = Tag::Type::Real;
    tag.unit = u"%"_s;
    tag.comment = u"Level of T-101, \"main\""_s;
    tag.minimum = 0;
    tag.maximum = 100;
    tag.simulation = Tag::Simulation::Sine;
    tag.period = 30;
    return tag;
}

} // namespace

class TagsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void valuesConvert()
    {
        QCOMPARE(TagValues::toNumber(TagValue(true)), std::optional(1.0));
        QCOMPARE(TagValues::toNumber(TagValue(u" 2.5 "_s)), std::optional(2.5));
        QVERIFY(!TagValues::toNumber(TagValue(u"abc"_s)));
        QVERIFY(TagValues::toBool(TagValue(u"on"_s)));
        QVERIFY(!TagValues::toBool(TagValue(0.0)));
        QCOMPARE(TagValues::toText(TagValue(qint64(42))), u"42"_s);

        Tag counter;
        counter.type = Tag::Type::Int;
        QCOMPARE(counter.converted(TagValue(2.6)), TagValue(qint64(3)));
        counter.minimum = 5;
        QCOMPARE(counter.startValue(), TagValue(qint64(5)));
        counter.initial = TagValue(qint64(9));
        QCOMPARE(counter.startValue(), TagValue(qint64(9)));
    }

    void jsonRoundTripKeepsUnknownKeys()
    {
        Tag tag = level();
        tag.initial = TagValue(12.5);
        QJsonObject json = tag.toJson();
        json.insert(u"scaling"_s, u"linear"_s);
        auto back = Tag::fromJson(json);
        QVERIFY(back);
        QCOMPARE(back->name, tag.name);
        QCOMPARE(back->initial, tag.initial);
        QCOMPARE(back->preserved.value(u"scaling").toString(), u"linear"_s);
        QCOMPARE(back->toJson(), json);

        QVERIFY(!Tag::fromJson(QJsonObject{{u"name"_s, u" bad "_s}}));
        QVERIFY(!Tag::fromJson(QJsonObject{{u"name"_s, u"T"_s}, {u"type"_s, u"qubit"_s}}));
    }

    void databaseKeepsNamesUnique()
    {
        Project project;
        TagDatabase& db = project.tags();
        QSignalSpy added(&project.notifier(), &DocumentNotifier::tagAdded);
        QSignalSpy changed(&project.notifier(), &DocumentNotifier::tagChanged);
        QSignalSpy removed(&project.notifier(), &DocumentNotifier::tagRemoved);

        QVERIFY(db.insert(level()));
        QCOMPARE(added.count(), 1);
        Tag same = level();
        same.name = u"tank_level"_s; // case-insensitive
        QCOMPARE(db.insert(same).error().code, ErrorCode::Duplicate);
        Tag empty;
        QVERIFY(!db.insert(empty));
        QVERIFY(db.find(u"TANK_LEVEL"_s));
        QCOMPARE(db.uniqueName(u"Tank_Level"_s), u"Tank_Level_2"_s);

        Tag renamed = level();
        renamed.name = u"Level"_s;
        QVERIFY(db.replace(u"Tank_Level"_s, renamed));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.front().at(0).toString(), u"Tank_Level"_s);
        QCOMPARE(changed.front().at(1).toString(), u"Level"_s);

        auto taken = db.take(u"level"_s);
        QVERIFY(taken);
        QCOMPARE(taken->name, u"Level"_s);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(db.size(), std::size_t(0));
    }

    void csvRoundTrip()
    {
        Tag pump;
        pump.name = u"Pump, P-101"_s;
        pump.type = Tag::Type::Bool;
        pump.simulation = Tag::Simulation::Toggle;
        pump.period = 8;
        const std::vector<Tag> tags{level(), pump};
        const QByteArray csv = TagCsv::write(tags);
        QVERIFY(csv.startsWith("name,type,unit,"));
        QVERIFY(csv.contains("\"Pump, P-101\""));
        auto back = TagCsv::read(csv);
        QVERIFY2(back, qPrintable(back ? QString() : back.error().message));
        QCOMPARE(back->size(), std::size_t(2));
        QCOMPARE((*back)[0].comment, level().comment);
        QCOMPARE((*back)[1].name, u"Pump, P-101"_s);
        QCOMPARE((*back)[1].simulation, Tag::Simulation::Toggle);
    }

    void csvFromSpreadsheets()
    {
        // Semicolons, columns in another order, a byte order mark and an
        // unknown column.
        const QByteArray csv = "\xEF\xBB\xBF"
                               "Comment;Name;Type;Colour;Maximum\r\n"
                               "Speed of M-1;Motor_Speed;int;red;1500\r\n";
        auto tags = TagCsv::read(csv);
        QVERIFY(tags);
        QCOMPARE(tags->size(), std::size_t(1));
        QCOMPARE(tags->front().name, u"Motor_Speed"_s);
        QCOMPARE(tags->front().type, Tag::Type::Int);
        QCOMPARE(tags->front().maximum, 1500.0);

        QVERIFY(!TagCsv::read("type,unit\nreal,%\n")); // no name column
        auto bad = TagCsv::read("name,type\nA,qubit\n");
        QVERIFY(!bad);
        QVERIFY(bad.error().message.contains(u"Row 2"));
    }

    void projectFilesKeepTags()
    {
        Project project(u"P"_s);
        (void)project.insertScreen(ScreenBuilder(u"S"_s).build());
        QVERIFY(project.tags().insert(level()));
        QTemporaryDir dir;
        QVERIFY(ProjectSerializer::save(project, dir.path()));
        auto loaded = ProjectSerializer::load(dir.path());
        QVERIFY(loaded);
        QCOMPARE((*loaded)->tags().size(), std::size_t(1));
        QCOMPARE((*loaded)->tags().tags().front(), level());
    }
};

QTEST_MAIN(TagsTest)
#include "TagsTest.moc"
