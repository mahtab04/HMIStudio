#include "comm/PlcDriver.h"

#include "comm/TagEngine.h"

#include <QHash>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{
Connection connection(const QString& driver)
{
    Connection result;
    result.id = driver;
    result.name = driver;
    result.driver = driver;
    result.host = u"fixture.invalid"_s;
    result.library = QString::fromUtf8(HMI_FAKE_PLC_LIBRARY);
    return result;
}

Tag tag(const QString& name, Tag::Type type, const QString& source, const QString& address)
{
    Tag result;
    result.name = name;
    result.type = type;
    result.connection = source;
    result.address = address;
    return result;
}
} // namespace

class PlcDriverTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void addressContracts()
    {
        const Connection s7 = connection(u"s7"_s);
        QVERIFY(
            PlcDriver::validateAddress(s7, tag(u"B"_s, Tag::Type::Bool, s7.id, u"DB1.DBX0.7"_s)));
        QVERIFY(
            !PlcDriver::validateAddress(s7, tag(u"B"_s, Tag::Type::Bool, s7.id, u"DB1.DBD0"_s)));
        QVERIFY(
            !PlcDriver::validateAddress(s7, tag(u"B"_s, Tag::Type::Real, s7.id, u"DB1.DBW0"_s)));
        QVERIFY(!PlcDriver::validateAddress(
            s7, tag(u"B"_s, Tag::Type::Int, s7.id, u"DB999999999999.DBD0"_s)));
        const Connection ab = connection(u"ab-eip"_s);
        auto attributes =
            PlcDriver::attributes(ab, tag(u"Speed"_s, Tag::Type::Real, ab.id, u"Motor.Speed"_s));
        QVERIFY(attributes);
        QVERIFY(attributes->contains(u"plc=controllogix"));
        QVERIFY(!PlcDriver::attributes(
            ab, tag(u"Speed"_s, Tag::Type::Real, ab.id, u"Speed&gateway=other"_s)));
        const Connection modbus = connection(u"modbus-tcp"_s);
        QVERIFY(PlcDriver::attributes(modbus, tag(u"R"_s, Tag::Type::Real, modbus.id, u"hr0"_s))
                    ->contains(u"elem_count=2"));
        QVERIFY(
            !PlcDriver::validateAddress(modbus, tag(u"B"_s, Tag::Type::Bool, modbus.id, u"hr0"_s)));
        QVERIFY(!PlcDriver::validateAddress(modbus,
                                            tag(u"R"_s, Tag::Type::Real, modbus.id, u"hr65535"_s)));
    }

    void readsThroughNativeAdapters()
    {
        const std::vector<Connection> connections{connection(u"s7"_s), connection(u"ab-eip"_s),
                                                  connection(u"modbus-tcp"_s)};
        std::vector<Tag> tags{tag(u"Bit"_s, Tag::Type::Bool, u"s7"_s, u"DB1.DBX0.0"_s),
                              tag(u"Dint"_s, Tag::Type::Int, u"s7"_s, u"DB1.DBD0"_s),
                              tag(u"Real"_s, Tag::Type::Real, u"s7"_s, u"DB1.DBD4"_s),
                              tag(u"Ab"_s, Tag::Type::Int, u"ab-eip"_s, u"Counter"_s),
                              tag(u"Mb"_s, Tag::Type::Int, u"modbus-tcp"_s, u"hr0"_s),
                              tag(u"Local"_s, Tag::Type::Real, {}, {})};
        tags.back().initial = 5.0;
        PlcDriver driver(connections);
        QVERIFY(driver.start(tags));
        QHash<QString, TagValue> values;
        const auto collect = [&](const QString& name, const TagValue& value)
        {
            values.insert(name, value);
        };
        driver.poll(0, collect);
        QCOMPARE(values.value(u"Bit"_s), TagValue(true));
        QCOMPARE(values.value(u"Dint"_s), TagValue(qint64(0x0100002A)));
        QCOMPARE(values.value(u"Real"_s), TagValue(12.5));
        QCOMPARE(values.value(u"Ab"_s), TagValue(qint64(42)));
        QCOMPARE(values.value(u"Mb"_s), TagValue(qint64(-123)));
        QCOMPARE(values.value(u"Local"_s), TagValue(5.0));
        QVERIFY(driver.problem(u"Real"_s).isEmpty());
        values.clear();
        driver.poll(0.1, collect);
        QVERIFY(values.isEmpty());
        driver.poll(0.5, collect);
        QCOMPARE(values.size(), 6);
        QVERIFY(!driver.write(u"Ab"_s, qint64(7)));
        driver.stop();
        QVERIFY(driver.start(tags));
        driver.poll(0, collect);
        QCOMPARE(values.size(), 6);
    }

    void failuresRemainVisible()
    {
        Connection missing = connection(u"ab-eip"_s);
        missing.library = u"/missing/hmi-test-driver-library"_s;
        const std::vector<Tag> tags{
            tag(u"Unavailable"_s, Tag::Type::Int, missing.id, u"Counter"_s)};
        TagEngine engine;
        engine.start(tags, std::make_unique<PlcDriver>(std::vector{missing}));
        QVERIFY(!engine.isSimulating());
        QTRY_COMPARE(engine.quality(u"Unavailable"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"Unavailable"_s).contains(u"Cannot load"));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.write(u"Unavailable"_s, qint64(9));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.stop();
        QCOMPARE(engine.quality(u"Unavailable"_s), u"Offline"_s);

        const std::vector<Tag> bad{tag(u"BadRead"_s, Tag::Type::Int, u"s7"_s, u"DB999.DBD0"_s)};
        engine.start(bad, std::make_unique<PlcDriver>(std::vector{connection(u"s7"_s)}));
        QTRY_COMPARE(engine.quality(u"BadRead"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"BadRead"_s).contains(u"1234"));
        engine.stop();
    }

    void queuedValuesStayInTheirRun()
    {
        TagEngine engine;
        const std::vector<Tag> external{tag(u"Value"_s, Tag::Type::Int, u"ab-eip"_s, u"Counter"_s)};
        engine.start(external, std::make_unique<PlcDriver>(std::vector{connection(u"ab-eip"_s)}));
        // Leave the native value queued while the GUI thread sleeps.
        QTest::qSleep(120);
        engine.stop();
        Tag local = tag(u"Value"_s, Tag::Type::Int, {}, {});
        local.initial = qint64(7);
        engine.start(std::vector{local});
        QCoreApplication::processEvents();
        QCOMPARE(engine.value(u"Value"_s), TagValue(qint64(7)));
        engine.stop();
    }
};

QTEST_MAIN(PlcDriverTest)
#include "PlcDriverTest.moc"
