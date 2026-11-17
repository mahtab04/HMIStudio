// The alarm engine: WinCC Unified's alarm statuses per state machine,
// discrete and analog triggers, acknowledgment, confirmation and the log.

#include "comm/AlarmEngine.h"

#include "comm/TagEngine.h"

#include <QSignalSpy>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

namespace
{

Tag tag(const QString& name, Tag::Type type, TagValue initial)
{
    Tag t;
    t.name = name;
    t.type = type;
    t.minimum = -1000;
    t.maximum = 1000;
    t.initial = std::move(initial);
    return t;
}

Alarm discrete(int id, const QString& tagName, int bit, const QString& alarmClass,
               bool fallingEdge = false)
{
    Alarm a;
    a.id = id;
    a.name = u"D%1"_s.arg(id);
    a.text = u"Discrete %1"_s.arg(id);
    a.tag = tagName;
    a.bit = bit;
    a.fallingEdge = fallingEdge;
    a.alarmClass = alarmClass;
    return a;
}

Alarm analog(int id, const QString& tagName, Alarm::Mode mode, double limit,
             const QString& alarmClass)
{
    Alarm a;
    a.id = id;
    a.kind = Alarm::Kind::Analog;
    a.name = u"A%1"_s.arg(id);
    a.text = u"Level {Level} % too high"_s;
    a.tag = tagName;
    a.mode = mode;
    a.limit = limit;
    a.alarmClass = alarmClass;
    return a;
}

} // namespace

class AlarmEngineTest : public QObject
{
    Q_OBJECT

private:
    TagEngine m_tags;
    AlarmEngine m_alarms{m_tags};

    void startWith(std::vector<Alarm> alarms)
    {
        m_tags.start(std::vector<Tag>{tag(u"Faults"_s, Tag::Type::Int, qint64(0)),
                                      tag(u"Door"_s, Tag::Type::Bool, true),
                                      tag(u"Level"_s, Tag::Type::Real, 50.0)});
        AlarmConfiguration configuration;
        configuration.alarms = std::move(alarms);
        m_alarms.start(configuration);
    }
    AlarmStatus statusOf(int id) const
    {
        const PendingAlarm* alarm = m_alarms.find(id);
        return alarm ? alarm->status : AlarmStatus::Normal;
    }

private Q_SLOTS:
    void cleanup()
    {
        m_alarms.stop();
        m_tags.stop();
    }

    void statusTables()
    {
        using M = AlarmStateMachine;
        using S = AlarmStatus;
        // Without acknowledgment: Incoming while active, then Normal.
        QCOMPARE(AlarmEngine::statusOf(M::WithoutAcknowledgment, true, false, false), S::Incoming);
        QCOMPARE(AlarmEngine::statusOf(M::WithoutAcknowledgment, false, false, false), S::Normal);
        // Simple acknowledgment.
        QCOMPARE(AlarmEngine::statusOf(M::SimpleAcknowledgment, true, false, false), S::Incoming);
        QCOMPARE(AlarmEngine::statusOf(M::SimpleAcknowledgment, false, false, false),
                 S::IncomingOutgoing);
        QCOMPARE(AlarmEngine::statusOf(M::SimpleAcknowledgment, true, true, true),
                 S::IncomingAcknowledged);
        QCOMPARE(AlarmEngine::statusOf(M::SimpleAcknowledgment, false, true, true), S::Normal);
        // Acknowledgment and confirmation.
        QCOMPARE(AlarmEngine::statusOf(M::AcknowledgmentAndConfirmation, false, true, false),
                 S::IncomingOutgoingAcknowledged);
        QCOMPARE(AlarmEngine::statusOf(M::AcknowledgmentAndConfirmation, false, true, true),
                 S::IncomingAcknowledgedOutgoing);
    }

    void discreteAlarmWithSimpleAcknowledgment()
    {
        startWith({discrete(1, u"Faults"_s, 2, u"Alarm"_s)});
        QVERIFY(m_alarms.pending().empty());
        QSignalSpy changed(&m_alarms, &AlarmEngine::changed);

        m_tags.write(u"Faults"_s, qint64(4)); // bit 2
        QTRY_COMPARE(statusOf(1), AlarmStatus::Incoming);
        QCOMPARE(m_alarms.unacknowledgedCount(), 1);
        QCOMPARE(m_alarms.pending().front().priority, 12); // the Alarm class
        m_tags.write(u"Faults"_s, qint64(1));              // bit 2 clear
        QTRY_COMPARE(statusOf(1), AlarmStatus::IncomingOutgoing);
        QVERIFY(m_alarms.acknowledge(1));
        QCOMPARE(statusOf(1), AlarmStatus::Normal); // gone and acknowledged
        QVERIFY(m_alarms.pending().empty());
        QVERIFY(!m_alarms.acknowledge(1));
        QVERIFY(changed.count() >= 3);
        // Logged, the newest first: incoming, incoming/outgoing, normal.
        QCOMPARE(m_alarms.log().size(), std::size_t(3));
        QCOMPARE(m_alarms.log().front().status, AlarmStatus::Normal);
        QCOMPARE(m_alarms.log().back().status, AlarmStatus::Incoming);
    }

    void fallingEdge()
    {
        startWith({discrete(2, u"Door"_s, 0, u"Warning"_s, true)});
        QVERIFY(m_alarms.pending().empty()); // the door is closed (true)
        m_tags.write(u"Door"_s, false);
        QTRY_COMPARE(statusOf(2), AlarmStatus::Incoming);
        QVERIFY(m_alarms.acknowledge(2)); // while active
        QCOMPARE(statusOf(2), AlarmStatus::IncomingAcknowledged);
        m_tags.write(u"Door"_s, true);
        QTRY_VERIFY(m_alarms.pending().empty());
    }

    void analogAlarmTextAndNotification()
    {
        startWith({analog(3, u"Level"_s, Alarm::Mode::Greater, 80, u"Notification"_s),
                   analog(4, u"Level"_s, Alarm::Mode::LessOrEqual, 10, u"Warning"_s)});
        m_tags.write(u"Level"_s, 85.0);
        QTRY_COMPARE(statusOf(3), AlarmStatus::Incoming);
        QCOMPARE(m_alarms.find(3)->text,
                 u"Level %1 % too high"_s.arg(QLocale().toString(85.0, 'f', 1)));
        QVERIFY(!m_alarms.acknowledge(3)); // notifications need no acknowledgment
        QCOMPARE(m_alarms.unacknowledgedCount(), 0);
        m_tags.write(u"Level"_s, 10.0);
        QTRY_VERIFY(!m_alarms.find(3));                   // gone: normal at once
        QTRY_COMPARE(statusOf(4), AlarmStatus::Incoming); // 10 <= 10
    }

    void acknowledgmentAndConfirmation()
    {
        startWith({discrete(5, u"Faults"_s, 0, u"AlarmWithReset"_s),
                   discrete(6, u"Faults"_s, 1, u"AlarmWithReset"_s)});
        m_tags.write(u"Faults"_s, qint64(3));
        QTRY_COMPARE(statusOf(6), AlarmStatus::Incoming);
        // 5: acknowledged while active, then gone; 6: gone, then acknowledged.
        QVERIFY(m_alarms.acknowledge(5));
        QVERIFY(!m_alarms.confirm(5)); // still active
        m_tags.write(u"Faults"_s, qint64(0));
        QTRY_COMPARE(statusOf(5), AlarmStatus::IncomingAcknowledgedOutgoing);
        QCOMPARE(statusOf(6), AlarmStatus::IncomingOutgoing);
        QVERIFY(!m_alarms.confirm(6)); // not acknowledged yet
        QVERIFY(m_alarms.acknowledge(6));
        QCOMPARE(statusOf(6), AlarmStatus::IncomingOutgoingAcknowledged);
        QVERIFY(m_alarms.confirm(5));
        QVERIFY(m_alarms.confirm(6));
        QVERIFY(m_alarms.pending().empty());
    }

    void groupAcknowledgmentAndRestart()
    {
        startWith({discrete(7, u"Faults"_s, 0, u"Alarm"_s), discrete(8, u"Faults"_s, 1, u"Alarm"_s),
                   discrete(9, u"Faults"_s, 2, u"Warning"_s)});
        m_tags.write(u"Faults"_s, qint64(7));
        QTRY_COMPARE(m_alarms.pending().size(), std::size_t(3));
        QCOMPARE(m_alarms.unacknowledgedCount(), 3);
        // The visible ones: 7 and 9.
        QCOMPARE(m_alarms.acknowledge(std::vector{7, 9}), 2);
        QCOMPARE(m_alarms.unacknowledgedCount(), 1);
        QCOMPARE(statusOf(8), AlarmStatus::Incoming);
        // An alarm that comes again while pending comes anew.
        m_tags.write(u"Faults"_s, qint64(6));
        QTRY_COMPARE(statusOf(7), AlarmStatus::Normal);
        m_tags.write(u"Faults"_s, qint64(7));
        QTRY_COMPARE(statusOf(7), AlarmStatus::Incoming);
        // Stopping the tags ends the alarms.
        m_tags.stop();
        QVERIFY(m_alarms.pending().empty());
        QVERIFY(!m_alarms.isRunning());
    }
};

QTEST_MAIN(AlarmEngineTest)
#include "AlarmEngineTest.moc"
