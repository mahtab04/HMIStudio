// PLC tags: S7 and Modbus TCP through the built-in clients, end to end
// against the built-in test PLC (real sockets on 127.0.0.1); Allen-Bradley
// through a deterministic libplctag fixture.

#include "comm/PlcDriver.h"

#include "comm/RuntimeDriver.h"
#include "comm/SimulatorDriver.h"
#include "comm/TagEngine.h"
#include "comm/TestPlc.h"

#include <QFileInfo>
#include <QHash>
#include <QTcpServer>
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
    if (driver == u"ab-eip")
        result.library = QString::fromUtf8(HMI_FAKE_PLC_LIBRARY);
    return result;
}

Connection local(const QString& driver, quint16 port)
{
    Connection result = connection(driver);
    result.host = u"127.0.0.1"_s;
    result.port = port;
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

using Values = QHash<QString, TagValue>;

// One poll: what the driver reported.
Values pollAll(PlcDriver& driver, double seconds)
{
    Values values;
    driver.poll(seconds,
                [&](const QString& name, const TagValue& value) { values.insert(name, value); });
    return values;
}
} // namespace

class PlcDriverTest : public QObject
{
    Q_OBJECT

private:
    TestPlc m_plc;

private Q_SLOTS:
    void initTestCase()
    {
        // Free ports, so tests never clash with a PLC or another run.
        auto started = m_plc.start(0, 0);
        QVERIFY2(started, started ? "" : qPrintable(started.error().message));
        QVERIFY(m_plc.s7Port() > 0 && m_plc.modbusPort() > 0);
    }
    void cleanupTestCase() { m_plc.stop(); }

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
        QVERIFY(
            PlcDriver::validateAddress(modbus, tag(u"R"_s, Tag::Type::Real, modbus.id, u"hr0"_s)));
        QVERIFY(
            !PlcDriver::validateAddress(modbus, tag(u"B"_s, Tag::Type::Bool, modbus.id, u"hr0"_s)));
        QVERIFY(!PlcDriver::validateAddress(modbus,
                                            tag(u"R"_s, Tag::Type::Real, modbus.id, u"hr65535"_s)));
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
            QVERIFY(!PlcDriver::typeForAddress(u"s7"_s, wrong));
        QCOMPARE(PlcDriver::typeForAddress(u"modbus-tcp"_s, u"co3"_s), Tag::Type::Bool);
        QCOMPARE(PlcDriver::typeForAddress(u"modbus-tcp"_s, u"hr7"_s), Tag::Type::Int);
        QVERIFY(!PlcDriver::typeForAddress(u"ab-eip"_s, u"Motor.Speed"_s));
        // The port: the protocol's unless given.
        QCOMPARE(s7.effectivePort(), 102);
        QCOMPARE(connection(u"modbus-tcp"_s).effectivePort(), 502);
        Connection ab = connection(u"ab-eip"_s);
        QCOMPARE(ab.effectivePort(), 44818);
        ab.port = 2222;
        QVERIFY(PlcDriver::attributes(ab, tag(u"C"_s, Tag::Type::Int, ab.id, u"Count"_s))
                    ->contains(u"gateway=fixture.invalid:2222"));
    }

    void s7ReadsAndWrites()
    {
        // Every S7 area and size, written and read back over the protocol.
        const std::vector<Connection> connections{local(u"s7"_s, m_plc.s7Port())};
        const std::vector<Tag> tags{tag(u"Flag"_s, Tag::Type::Bool, u"s7"_s, u"M1.3"_s),
                                    tag(u"Other"_s, Tag::Type::Bool, u"s7"_s, u"M1.4"_s),
                                    tag(u"Byte"_s, Tag::Type::Int, u"s7"_s, u"MB2"_s),
                                    tag(u"Word"_s, Tag::Type::Int, u"s7"_s, u"MW4"_s),
                                    tag(u"DInt"_s, Tag::Type::Int, u"s7"_s, u"DB3.DBD0"_s),
                                    tag(u"Speed"_s, Tag::Type::Real, u"s7"_s, u"DB3.DBD4"_s),
                                    tag(u"Out"_s, Tag::Type::Bool, u"s7"_s, u"%Q0.1"_s),
                                    tag(u"In"_s, Tag::Type::Int, u"s7"_s, u"IW2"_s)};
        PlcDriver driver(connections);
        QVERIFY(driver.start(tags));
        Values values = pollAll(driver, 0);
        QCOMPARE(values.size(), int(tags.size()));
        QVERIFY2(driver.problem(u"Flag"_s).isEmpty(), qPrintable(driver.problem(u"Flag"_s)));
        QCOMPARE(values.value(u"Flag"_s), TagValue(false));
        QCOMPARE(values.value(u"Word"_s), TagValue(qint64(0)));

        QVERIFY(driver.write(u"Flag"_s, true));
        QVERIFY(driver.write(u"Byte"_s, qint64(300))); // clamped to a byte
        QVERIFY(driver.write(u"Word"_s, qint64(-1234)));
        QVERIFY(driver.write(u"DInt"_s, qint64(100000)));
        QVERIFY(driver.write(u"Speed"_s, 3.5));
        QVERIFY(driver.write(u"Out"_s, true));
        QVERIFY(driver.write(u"In"_s, qint64(9)));
        // Read back at once, on the next poll; the others at their cycle.
        values = pollAll(driver, 0.05);
        QCOMPARE(values.value(u"Flag"_s), TagValue(true));
        QVERIFY(!values.contains(u"Other"_s));
        values = pollAll(driver, 1.0);
        QCOMPARE(values.value(u"Other"_s), TagValue(false)); // its neighbour bit untouched
        QCOMPARE(values.value(u"Byte"_s), TagValue(qint64(255)));
        QCOMPARE(values.value(u"Word"_s), TagValue(qint64(-1234)));
        QCOMPARE(values.value(u"DInt"_s), TagValue(qint64(100000)));
        QCOMPARE(values.value(u"Speed"_s), TagValue(3.5));
        QCOMPARE(values.value(u"Out"_s), TagValue(true));
        QCOMPARE(values.value(u"In"_s), TagValue(qint64(9)));
        QVERIFY(!driver.write(u"Nothing"_s, qint64(1)));

        // Out of the test PLC's 1024 marker bytes: the PLC says so.
        driver.stop();
        QVERIFY(driver.start(std::vector{tag(u"Far"_s, Tag::Type::Int, u"s7"_s, u"MW5000"_s)}));
        values = pollAll(driver, 0);
        QVERIFY(std::holds_alternative<std::monostate>(values.value(u"Far"_s)));
        QVERIFY2(driver.problem(u"Far"_s).contains(u"out of range"),
                 qPrintable(driver.problem(u"Far"_s)));
        driver.stop();
    }

    void testPlcHeartbeat()
    {
        // MW100 counts seconds.
        PlcDriver driver(std::vector{local(u"s7"_s, m_plc.s7Port())});
        QVERIFY(driver.start(std::vector{tag(u"Seconds"_s, Tag::Type::Int, u"s7"_s, u"MW100"_s)}));
        const TagValue first = pollAll(driver, 0).value(u"Seconds"_s);
        QVERIFY(std::holds_alternative<qint64>(first));
        double seconds = 1.0;
        QTRY_VERIFY_WITH_TIMEOUT(pollAll(driver, seconds += 1.0).value(u"Seconds"_s) != first,
                                 3000);
        driver.stop();
    }

    void modbusReadsAndWrites()
    {
        const std::vector<Connection> connections{local(u"modbus-tcp"_s, m_plc.modbusPort())};
        const std::vector<Tag> tags{tag(u"Coil"_s, Tag::Type::Bool, u"modbus-tcp"_s, u"co5"_s),
                                    tag(u"Reg"_s, Tag::Type::Int, u"modbus-tcp"_s, u"hr4"_s),
                                    tag(u"Real"_s, Tag::Type::Real, u"modbus-tcp"_s, u"hr10"_s),
                                    tag(u"Input"_s, Tag::Type::Int, u"modbus-tcp"_s, u"ir3"_s)};
        PlcDriver driver(connections);
        QVERIFY(driver.start(tags));
        Values values = pollAll(driver, 0);
        QVERIFY2(driver.problem(u"Coil"_s).isEmpty(), qPrintable(driver.problem(u"Coil"_s)));
        QCOMPARE(values.value(u"Coil"_s), TagValue(false));
        QVERIFY(driver.write(u"Coil"_s, true));
        QVERIFY(driver.write(u"Reg"_s, qint64(-7)));
        QVERIFY(driver.write(u"Real"_s, 12.5));        // two registers, FC16
        QVERIFY(!driver.write(u"Input"_s, qint64(1))); // input registers are read only
        values = pollAll(driver, 0.05);
        QCOMPARE(values.value(u"Coil"_s), TagValue(true));
        QCOMPARE(values.value(u"Reg"_s), TagValue(qint64(-7)));
        QCOMPARE(values.value(u"Real"_s), TagValue(12.5));
        QCOMPARE(values.value(u"Input"_s), TagValue(qint64(0)));
        driver.stop();
    }

    void allenBradleyThroughLibplctag()
    {
        PlcDriver driver(std::vector{connection(u"ab-eip"_s)}, trusted());
        std::vector<Tag> tags{tag(u"Count"_s, Tag::Type::Int, u"ab-eip"_s, u"Count"_s),
                              tag(u"Local"_s, Tag::Type::Real, {}, {})};
        tags.back().initial = 5.0;
        QVERIFY(driver.start(tags));
        Values values = pollAll(driver, 0);
        QCOMPARE(values.value(u"Count"_s), TagValue(qint64(42)));
        QCOMPARE(values.value(u"Local"_s), TagValue(5.0));
        QVERIFY(pollAll(driver, 0.1).isEmpty()); // acquisition interval
        QVERIFY(driver.write(u"Count"_s, qint64(7)));
        QCOMPARE(pollAll(driver, 0.15).value(u"Count"_s), TagValue(qint64(7)));
        driver.stop();
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
        QVERIFY(engine.problem(u"Unavailable"_s).contains(u"libplctag"));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.write(u"Unavailable"_s, qint64(9));
        QVERIFY(std::holds_alternative<std::monostate>(engine.value(u"Unavailable"_s)));
        engine.stop();
        QCOMPARE(engine.quality(u"Unavailable"_s), u"Offline"_s);

        // A PLC that does not answer: Bad, with the reason, retried later.
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost));
        const quint16 port = closed.serverPort();
        closed.close();
        const std::vector<Tag> offline{tag(u"Offline"_s, Tag::Type::Int, u"s7"_s, u"MW0"_s)};
        engine.start(offline, std::make_unique<PlcDriver>(std::vector{local(u"s7"_s, port)}));
        QTRY_COMPARE(engine.quality(u"Offline"_s), u"Bad"_s);
        QVERIFY(engine.problem(u"Offline"_s).contains(u"No connection to the PLC"));
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

    void runtimeMixesSimulationAndPlcs()
    {
        // Internal tags simulated, PLC tags from the PLC, in one run.
        Tag internal = tag(u"Local"_s, Tag::Type::Real, {}, {});
        internal.initial = 5.0;
        const std::vector<Tag> tags{internal, tag(u"Plc"_s, Tag::Type::Int, u"s7"_s, u"MW12"_s)};
        TagEngine engine;
        engine.start(tags, std::make_unique<RuntimeDriver>(std::make_unique<SimulatorDriver>(),
                                                           std::make_unique<PlcDriver>(std::vector{
                                                               local(u"s7"_s, m_plc.s7Port())})));
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
        Tag internal = tag(u"Value"_s, Tag::Type::Int, {}, {});
        internal.initial = qint64(7);
        engine.start(std::vector{internal});
        QCoreApplication::processEvents();
        QCOMPARE(engine.value(u"Value"_s), TagValue(qint64(7)));
        engine.stop();
    }
};

QTEST_MAIN(PlcDriverTest)
#include "PlcDriverTest.moc"
