#include "comm/PlcDriver.h"

#include "comm/RuntimeDriver.h"
#include "comm/SimulatorDriver.h"
#include "comm/TagEngine.h"

#include <QFileInfo>
#include <QHash>
#include <QLibrary>
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

// The fixture library, as if the user had chosen it in Connections.
QStringList trusted()
{
    return {QString::fromUtf8(HMI_FAKE_PLC_LIBRARY)};
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
        PlcDriver driver(connections, trusted());
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
        engine.start(
            tags, std::make_unique<PlcDriver>(std::vector{missing}, QStringList{missing.library}));
        QVERIFY(!engine.isSimulating());
        QTRY_COMPARE(engine.quality(u"Unavailable"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"Unavailable"_s).contains(u"Cannot load"));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.write(u"Unavailable"_s, qint64(9));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.stop();
        QCOMPARE(engine.quality(u"Unavailable"_s), u"Offline"_s);

        const std::vector<Tag> bad{tag(u"BadRead"_s, Tag::Type::Int, u"s7"_s, u"DB999.DBD0"_s)};
        engine.start(bad, std::make_unique<PlcDriver>(std::vector{connection(u"s7"_s)}, trusted()));
        QTRY_COMPARE(engine.quality(u"BadRead"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"BadRead"_s).contains(u"1234"));
        engine.stop();
    }

    void projectLibrariesNeedTrust()
    {
        // A library named by the project is not loaded until the user chose
        // it on this computer.
        const Connection ab = connection(u"ab-eip"_s);
        const std::vector<Tag> tags{tag(u"Counter"_s, Tag::Type::Int, ab.id, u"Counter"_s)};
        TagEngine engine;
        engine.start(tags, std::make_unique<PlcDriver>(std::vector{ab}));
        QTRY_COMPARE(engine.quality(u"Counter"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"Counter"_s).contains(u"Browse"));
        engine.stop();

        QVERIFY(PlcDriver::isTrustedLibrary(ab.library, trusted()));
        // Paths are compared clean and absolute.
        const QFileInfo file(ab.library);
        QVERIFY(PlcDriver::isTrustedLibrary(
            ab.library, QStringList{file.absolutePath() + u"/x/../"_s + file.fileName()}));
        QVERIFY(!PlcDriver::isTrustedLibrary(u"//server/share/evil.dll"_s, trusted()));
        QVERIFY(!PlcDriver::isTrustedLibrary(QString(), trusted()));
    }

    void s7AddressesAndTypes()
    {
        const Connection s7 = connection(u"s7"_s);
        // TIA Portal's forms: with or without %, markers, inputs, outputs.
        for (const auto& [address, type] :
             std::vector<std::pair<QString, Tag::Type>>{{u"%DB1.DBX0.0"_s, Tag::Type::Bool},
                                                        {u"M0.0"_s, Tag::Type::Bool},
                                                        {u"%I0.7"_s, Tag::Type::Bool},
                                                        {u"Q1.0"_s, Tag::Type::Bool},
                                                        {u"%MW10"_s, Tag::Type::Int},
                                                        {u"MB3"_s, Tag::Type::Int},
                                                        {u"%IW64"_s, Tag::Type::Int},
                                                        {u"MD20"_s, Tag::Type::Real},
                                                        {u"DB2.DBD4"_s, Tag::Type::Real}})
        {
            QCOMPARE(PlcDriver::typeForAddress(u"s7"_s, address), type);
            QVERIFY2(PlcDriver::validateAddress(s7, tag(u"T"_s, type, s7.id, address)),
                     qPrintable(address));
        }
        // A DInt: an Int on a double word is fine too.
        QVERIFY(PlcDriver::validateAddress(s7, tag(u"T"_s, Tag::Type::Int, s7.id, u"MD20"_s)));
        for (const QString& wrong : {u"M0"_s, u"MW0.1"_s, u"DB1.DBX0"_s, u"X5"_s, u"DB0.DBW0"_s})
        {
            QVERIFY(!PlcDriver::typeForAddress(u"s7"_s, wrong));
        }
        QCOMPARE(PlcDriver::typeForAddress(u"modbus-tcp"_s, u"co3"_s), Tag::Type::Bool);
        QCOMPARE(PlcDriver::typeForAddress(u"modbus-tcp"_s, u"hr7"_s), Tag::Type::Int);
        QVERIFY(!PlcDriver::typeForAddress(u"ab-eip"_s, u"Motor.Speed"_s));
        // The port: the protocol's unless given.
        QCOMPARE(s7.effectivePort(), 102);
        Connection modbus = connection(u"modbus-tcp"_s);
        QCOMPARE(modbus.effectivePort(), 502);
        modbus.port = 1502;
        QVERIFY(PlcDriver::attributes(modbus, tag(u"R"_s, Tag::Type::Int, modbus.id, u"hr0"_s))
                    ->contains(u"gateway=fixture.invalid:1502"));
    }

    void writesThroughNativeAdapters()
    {
        Connection s7 = connection(u"s7"_s);
        s7.port = 1102;
        const std::vector<Connection> connections{s7, connection(u"ab-eip"_s)};
        const std::vector<Tag> tags{tag(u"Flag"_s, Tag::Type::Bool, u"s7"_s, u"M1.3"_s),
                                    tag(u"Word"_s, Tag::Type::Int, u"s7"_s, u"MW4"_s),
                                    tag(u"Speed"_s, Tag::Type::Real, u"s7"_s, u"MD8"_s),
                                    tag(u"Count"_s, Tag::Type::Int, u"ab-eip"_s, u"Count"_s)};
        PlcDriver driver(connections, trusted());
        QVERIFY(driver.start(tags));
        QHash<QString, TagValue> values;
        const auto collect = [&](const QString& name, const TagValue& value)
        {
            values.insert(name, value);
        };
        driver.poll(0, collect);
        QCOMPARE(values.value(u"Flag"_s), TagValue(false));
        QVERIFY(driver.write(u"Flag"_s, true));
        QVERIFY(driver.write(u"Word"_s, qint64(-1234)));
        QVERIFY(driver.write(u"Speed"_s, 3.5));
        QVERIFY(driver.write(u"Count"_s, qint64(7)));
        // Read back at once, on the next poll.
        values.clear();
        driver.poll(0.05, collect);
        QCOMPARE(values.value(u"Flag"_s), TagValue(true));
        QCOMPARE(values.value(u"Word"_s), TagValue(qint64(-1234)));
        QCOMPARE(values.value(u"Speed"_s), TagValue(3.5));
        QCOMPARE(values.value(u"Count"_s), TagValue(qint64(7)));
        // The port was set before connecting.
        QLibrary fixture(QString::fromUtf8(HMI_FAKE_PLC_LIBRARY));
        const auto port = reinterpret_cast<int (*)()>(fixture.resolve("fake_s7_port"));
        QVERIFY(port);
        QCOMPARE(port(), 1102);
        QVERIFY(!driver.write(u"Nothing"_s, qint64(1)));
        driver.stop();
    }

    void runtimeMixesSimulationAndPlcs()
    {
        // Internal tags simulated, PLC tags from the PLC, in one run.
        Tag local = tag(u"Local"_s, Tag::Type::Real, {}, {});
        local.initial = 5.0;
        const std::vector<Tag> tags{local, tag(u"Plc"_s, Tag::Type::Int, u"s7"_s, u"MW12"_s)};
        TagEngine engine;
        engine.start(tags,
                     std::make_unique<RuntimeDriver>(
                         std::make_unique<SimulatorDriver>(),
                         std::make_unique<PlcDriver>(std::vector{connection(u"s7"_s)}, trusted())));
        QCOMPARE(engine.quality(u"Local"_s), u"Simulated"_s);
        QCOMPARE(engine.value(u"Local"_s), TagValue(5.0));
        QVERIFY(engine.readsPlc(u"Plc"_s));
        QVERIFY(!engine.readsPlc(u"Local"_s));
        QTRY_COMPARE(engine.quality(u"Plc"_s), u"Good"_s);
        QCOMPARE(engine.value(u"Plc"_s), TagValue(qint64(0)));
        // An operator input reaches the PLC and shows once read back.
        QVERIFY(engine.writeChecked(u"Plc"_s, qint64(77)));
        QTRY_COMPARE(engine.value(u"Plc"_s), TagValue(qint64(77)));
        engine.write(u"Local"_s, 9.0);
        QCOMPARE(engine.value(u"Local"_s), TagValue(9.0));
        engine.stop();
    }

    void queuedValuesStayInTheirRun()
    {
        TagEngine engine;
        const std::vector<Tag> external{tag(u"Value"_s, Tag::Type::Int, u"ab-eip"_s, u"Counter"_s)};
        engine.start(external,
                     std::make_unique<PlcDriver>(std::vector{connection(u"ab-eip"_s)}, trusted()));
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
