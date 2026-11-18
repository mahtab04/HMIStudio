// The alarm engine: WinCC Unified's alarm statuses per state machine,
// discrete and analog triggers, acknowledgment, confirmation and the log.

#include "comm/AlarmEngine.h"

#include "comm/RuntimeAlarms.h"
#include "comm/TagEngine.h"
#include "core/objects/AlarmView.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
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

    void startWith(std::vector<Alarm> alarms, AlarmSettings settings = {})
    {
        m_tags.start(std::vector<Tag>{tag(u"Faults"_s, Tag::Type::Int, qint64(0)),
                                      tag(u"Door"_s, Tag::Type::Bool, true),
                                      tag(u"Level"_s, Tag::Type::Real, 50.0)});
        AlarmConfiguration configuration;
        configuration.alarms = std::move(alarms);
        configuration.settings = std::move(settings);
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

    void alarmControlAtRuntime()
    {
        startWith({discrete(7, u"Faults"_s, 0, u"Alarm"_s), discrete(8, u"Faults"_s, 1, u"Alarm"_s),
                   discrete(9, u"Faults"_s, 2, u"Warning"_s)});
        RuntimeAlarms alarms(m_alarms);
        m_tags.write(u"Faults"_s, qint64(7));
        QTRY_COMPARE(alarms.pendingCount(), 3);
        const auto rows = alarms.rows(AlarmList::Active);
        QCOMPARE(rows.size(), std::size_t(3));
        QCOMPARE(rows.front().status, u"Incoming"_s);
        // Alarm-class alarms come red and flashing; warnings yellow.
        const auto alarmRow = std::ranges::find(rows, 7, &AlarmRow::id);
        QVERIFY(alarmRow->colors.flashing);
        QCOMPARE(alarmRow->colors.background, QColor(0xD6, 0x45, 0x41));

        AlarmView view;
        const QRectF size = view.geometry();
        const auto press = [&](AlarmView::ToolbarButton button)
        {
            return view.operateAt(view.buttonRect(button).center(), &alarms);
        };

        // "Alarms - show current": the newest is the one acknowledged.
        const int newest = alarms.rows(AlarmList::Active).front().id;
        QVERIFY(press(AlarmView::Acknowledge));
        QCOMPARE(alarms.unacknowledgedCount(), 2);
        QCOMPARE(m_alarms.find(newest)->status, AlarmStatus::IncomingAcknowledged);
        // Moving the selection stops showing the newest.
        QVERIFY(press(AlarmView::NextLine));
        QVERIFY(!alarms.viewState(view.id()).showCurrent);
        QCOMPARE(alarms.viewState(view.id()).selected, 1);
        QVERIFY(view.operateAt(view.lineRect(2).center(), &alarms));
        QCOMPARE(alarms.viewState(view.id()).selected, 2);
        // Group acknowledgment of the visible alarms.
        QVERIFY(press(AlarmView::AcknowledgeVisible));
        QCOMPARE(alarms.unacknowledgedCount(), 0);
        // The log; acknowledging does nothing there.
        QVERIFY(press(AlarmView::ShowLogged));
        QCOMPARE(alarms.viewState(view.id()).list, AlarmList::Logged);
        QCOMPARE(alarms.rows(AlarmList::Logged).size(),
                 std::size_t(6)); // 3 incoming, 3 acknowledged
        QVERIFY(press(AlarmView::ShowActive));
        QVERIFY(press(AlarmView::ShowCurrent));
        QVERIFY(alarms.viewState(view.id()).showCurrent);
        QVERIFY(!view.operateAt(QPointF(-5, -5), &alarms)); // outside

        // Drawn live: the first line in the colours of its status.
        QImage image(size.size().toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        view.paint(painter, PaintContext{.zoom = 1.0,
                                         .editing = false,
                                         .graphics = nullptr,
                                         .screens = nullptr,
                                         .depth = 0,
                                         .alarms = &alarms});
        painter.end();
        const QRectF first = view.lineRect(0);
        const AlarmRow top = alarms.rows(AlarmList::Active).front();
        QCOMPARE(image.pixel(QPoint(int(first.left()) + 3, int(first.center().y()))),
                 top.colors.background.rgba());
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

    void alarmParameters()
    {
        // {n}: alarm parameter n; {Tag}: a tag; formats; unknown fields stay.
        Alarm a;
        a.name = u"Named"_s;
        a.parameters = {u"Level"_s, u"Count"_s};
        const Alarms::TagLookup values = [](const QString& tag) -> std::optional<TagValue>
        {
            if (tag == u"Level")
                return 7.25;
            if (tag == u"Count")
                return TagValue(qint64(-42));
            if (tag == u"Door")
                return true;
            return std::nullopt;
        };
        const QLocale locale;
        a.text = u"L={1} C={2} D={Door}"_s;
        QCOMPARE(Alarms::formatText(a, values),
                 u"L=%1 C=%2 D=true"_s.arg(locale.toString(7.25, 'f', 1), locale.toString(-42)));
        a.text = u"[{1:08.3}] [{2:-6}] [{2:6}] [{2:06}]"_s;
        QCOMPARE(Alarms::formatText(a, values),
                 u"[%1] [%2   ] [   %2] [-00042]"_s.arg(
                     locale.toString(7.25, 'f', 3).rightJustified(8, u'0'), locale.toString(-42)));
        a.text = u"{3} {Missing} {Level:.0}"_s;
        QCOMPARE(Alarms::formatText(a, values),
                 u"{3} {Missing} %1"_s.arg(locale.toString(7.25, 'f', 0)));
        a.text.clear();
        QCOMPARE(Alarms::formatText(a, values), u"Named"_s);

        // At runtime, from the parameters' tags.
        Alarm withParameter = discrete(10, u"Faults"_s, 0, u"Warning"_s);
        withParameter.text = u"Faults {1} at level {2:.2}"_s;
        withParameter.parameters = {u"Faults"_s, u"Level"_s};
        startWith({withParameter});
        m_tags.write(u"Faults"_s, qint64(1));
        QTRY_VERIFY(m_alarms.find(10));
        QCOMPARE(m_alarms.find(10)->text,
                 u"Faults 1 at level %1"_s.arg(locale.toString(50.0, 'f', 2)));
    }

    void statusTexts()
    {
        AlarmSettings settings;
        settings.statusTexts[std::size_t(AlarmStatus::Incoming)] = u"C"_s;
        startWith({discrete(11, u"Faults"_s, 0, u"Alarm"_s)}, settings);
        RuntimeAlarms alarms(m_alarms);
        m_tags.write(u"Faults"_s, qint64(1));
        QTRY_COMPARE(alarms.pendingCount(), 1);
        QCOMPARE(alarms.rows(AlarmList::Active).front().status, u"C"_s);
        QVERIFY(m_alarms.acknowledge(11)); // not configured: the default text
        QCOMPARE(alarms.rows(AlarmList::Active).front().status, u"Incoming/Acknowledged"_s);
        QCOMPARE(alarms.rows(AlarmList::Logged).back().status, u"C"_s);
    }

    void disableAndEnable()
    {
        startWith(
            {discrete(12, u"Faults"_s, 0, u"Alarm"_s), discrete(13, u"Faults"_s, 1, u"Alarm"_s)});
        RuntimeAlarms alarms(m_alarms);
        m_tags.write(u"Faults"_s, qint64(1));
        QTRY_COMPARE(alarms.pendingCount(), 1);
        const std::size_t logged = m_alarms.log().size();
        QVERIFY(m_alarms.disable(12));
        QVERIFY(!m_alarms.disable(12));
        QCOMPARE(alarms.pendingCount(), 0);      // gone from the list
        QCOMPARE(m_alarms.log().size(), logged); // not logged
        // Disabled alarms are not raised.
        m_tags.write(u"Faults"_s, qint64(0));
        m_tags.write(u"Faults"_s, qint64(1));
        QTest::qWait(150);
        QVERIFY(!m_alarms.find(12));
        const auto defined = alarms.rows(AlarmList::Defined);
        QCOMPARE(defined.size(), std::size_t(2));
        QCOMPARE(defined.front().id, 12);
        QVERIFY(!defined.front().enabled);
        QCOMPARE(defined.front().status, u"Disabled"_s);
        // Enabled again: checked, and it comes since its cause holds.
        QVERIFY(m_alarms.enable(12));
        QCOMPARE(statusOf(12), AlarmStatus::Incoming);
        QVERIFY(m_alarms.disable(13));
        QVERIFY(m_alarms.enable(13));
        QVERIFY(!m_alarms.find(13)); // its bit is clear
    }

    void shelving()
    {
        AlarmSettings settings;
        settings.shelvingTimeout = 1;
        startWith(
            {discrete(14, u"Faults"_s, 0, u"Alarm"_s), discrete(15, u"Faults"_s, 1, u"Warning"_s)},
            settings);
        RuntimeAlarms alarms(m_alarms);
        m_tags.write(u"Faults"_s, qint64(3));
        QTRY_COMPARE(alarms.pendingCount(), 2);
        QVERIFY(m_alarms.shelve(14));
        QVERIFY(!m_alarms.shelve(14));
        // Out of the active alarms, the counts and acknowledgment.
        QCOMPARE(alarms.pendingCount(), 1);
        QCOMPARE(alarms.unacknowledgedCount(), 1);
        QCOMPARE(alarms.rows(AlarmList::Active).front().id, 15);
        const auto defined = alarms.rows(AlarmList::Defined);
        QVERIFY(defined.front().shelved);
        QCOMPARE(defined.front().status, u"Shelved · Incoming"_s);
        // Still evaluated: it goes while shelved.
        m_tags.write(u"Faults"_s, qint64(2));
        QTRY_COMPARE(statusOf(14), AlarmStatus::IncomingOutgoing);
        QCOMPARE(alarms.pendingCount(), 1);
        // The shelving timeout unshelves it.
        QTRY_VERIFY_WITH_TIMEOUT(!m_alarms.find(14)->shelved, 4000);
        QCOMPARE(alarms.pendingCount(), 2);
        QCOMPARE(alarms.unacknowledgedCount(), 2);
        QVERIFY(m_alarms.shelve(15));
        QVERIFY(m_alarms.unshelve(15));
        QVERIFY(!m_alarms.unshelve(15));
    }

    void statistics()
    {
        // Oldest last, as the log: 1 came at 0 s, acknowledged at 4 s, went
        // at 10 s, came again at 30 s and went at 32 s; 2 only went.
        const QDateTime t0(QDate(2026, 1, 1), QTime(8, 0));
        std::vector<LoggedAlarm> log{
            {1, u"Again"_s, u"Alarm"_s, AlarmStatus::IncomingOutgoing, t0.addSecs(32),
             AlarmEvent::Went},
            {1, u"Again"_s, u"Alarm"_s, AlarmStatus::Incoming, t0.addSecs(30), AlarmEvent::Came},
            {2, u"Other"_s, u"Alarm"_s, AlarmStatus::Normal, t0.addSecs(12), AlarmEvent::Went},
            {1, u"First"_s, u"Alarm"_s, AlarmStatus::Normal, t0.addSecs(10), AlarmEvent::Went},
            {1, u"First"_s, u"Alarm"_s, AlarmStatus::IncomingAcknowledged, t0.addSecs(4),
             AlarmEvent::Acknowledged},
            {1, u"First"_s, u"Alarm"_s, AlarmStatus::Incoming, t0, AlarmEvent::Came},
        };
        const auto lines = AlarmEngine::statisticsOf(log);
        QCOMPARE(lines.size(), std::size_t(1)); // 2 never came
        const AlarmStatistics& s = lines.front();
        QCOMPARE(s.id, 1);
        QCOMPARE(s.text, u"Again"_s);
        QCOMPARE(s.frequency, 2);
        QCOMPARE(s.sumActiveActive, 30.0);
        QCOMPARE(s.sumActiveInactive, 12.0);
        QCOMPARE(s.averageActiveInactive(), 6.0);
        QCOMPARE(s.sumActiveAcknowledged, 4.0);
        QCOMPARE(s.averageActiveAcknowledged(), 4.0);
    }

    void logKeptAcrossRunsAndExport()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"logs/alarms.json"_s);
        m_alarms.setLogFile(file);
        startWith({discrete(16, u"Faults"_s, 0, u"Alarm"_s)});
        m_tags.write(u"Faults"_s, qint64(1));
        QTRY_VERIFY(m_alarms.find(16));
        QVERIFY(m_alarms.acknowledge(16));
        m_alarms.stop();
        QVERIFY(m_alarms.log().empty());
        QVERIFY(QFile::exists(file));

        // The next run starts with the log of the last; statistics use it.
        m_tags.stop();
        startWith({discrete(16, u"Faults"_s, 0, u"Alarm"_s)});
        QCOMPARE(m_alarms.log().size(), std::size_t(2));
        QCOMPARE(m_alarms.log().front().event, AlarmEvent::Acknowledged);
        RuntimeAlarms alarms(m_alarms);
        QCOMPARE(alarms.rows(AlarmList::Statistics).size(), std::size_t(1));
        QCOMPARE(alarms.rows(AlarmList::Statistics).front().statistics[AlarmRow::Frequency],
                 u"1"_s);

        // Export: CSV with a byte order mark and a header.
        alarms.setExportFolder(dir.filePath(u"exports"_s));
        QSignalSpy exported(&alarms, &RuntimeAlarms::exported);
        const QString path = alarms.exportList(AlarmList::Logged);
        QVERIFY(!path.isEmpty());
        QCOMPARE(exported.count(), 1);
        QFile csv(path);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QByteArray content = csv.readAll();
        QVERIFY(content.startsWith("\xEF\xBB\xBF"));
        const QStringList lines =
            QString::fromUtf8(content.mid(3)).split(u"\r\n"_s, Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.front().startsWith(u"Alarm number,Date,Time"_s));
        QVERIFY(lines.at(1).startsWith(u"16,"_s));
        QVERIFY(
            alarms.csvOf(AlarmList::Statistics).startsWith(u"Alarm number,Alarm text,Frequency"_s));
        m_alarms.setLogFile({});
    }

    void alarmControlListsAndOperatorActions()
    {
        startWith(
            {discrete(17, u"Faults"_s, 0, u"Alarm"_s), discrete(18, u"Faults"_s, 1, u"Alarm"_s)});
        RuntimeAlarms alarms(m_alarms);
        QTemporaryDir dir;
        alarms.setExportFolder(dir.path());
        m_tags.write(u"Faults"_s, qint64(3));
        QTRY_COMPARE(alarms.pendingCount(), 2);
        AlarmView view;
        const auto press = [&](AlarmView::ToolbarButton button)
        {
            return view.operateAt(view.buttonRect(button).center(), &alarms);
        };
        const auto list = [&]
        {
            return alarms.viewState(view.id()).list;
        };

        // Disable works on the defined alarms only.
        QVERIFY(press(AlarmView::Disable));
        QCOMPARE(alarms.pendingCount(), 2);
        // Shelve the newest active alarm.
        const int newest = alarms.rows(AlarmList::Active).front().id;
        QVERIFY(press(AlarmView::Shelve));
        QCOMPARE(alarms.pendingCount(), 1);
        QVERIFY(m_alarms.find(newest)->shelved);

        QVERIFY(press(AlarmView::ShowDefined));
        QCOMPARE(list(), AlarmList::Defined);
        QCOMPARE(alarms.rows(AlarmList::Defined).size(), std::size_t(2));
        // Select the shelved one and unshelve it, then disable and enable it.
        QVERIFY(view.operateAt(view.lineRect(newest == 17 ? 0 : 1).center(), &alarms));
        QVERIFY(press(AlarmView::Unshelve));
        QVERIFY(!m_alarms.find(newest)->shelved);
        QVERIFY(press(AlarmView::Disable));
        QVERIFY(!m_alarms.find(newest));
        QVERIFY(!alarms.rows(AlarmList::Defined)[newest == 17 ? 0 : 1].enabled);
        QVERIFY(press(AlarmView::Enable));
        QTRY_VERIFY(m_alarms.find(newest));

        QVERIFY(press(AlarmView::ShowStatistics));
        QCOMPARE(list(), AlarmList::Statistics);
        QCOMPARE(alarms.rows(AlarmList::Statistics).size(), std::size_t(2));
        QSignalSpy exported(&alarms, &RuntimeAlarms::exported);
        QVERIFY(press(AlarmView::Export));
        QCOMPARE(exported.count(), 1);
        QVERIFY(exported.front().front().toString().contains(u"AlarmStatistics_"_s));

        // Every list paints, with its columns.
        for (const AlarmView::ToolbarButton b : {AlarmView::ShowActive, AlarmView::ShowLogged,
                                                 AlarmView::ShowStatistics, AlarmView::ShowDefined})
        {
            QVERIFY(press(b));
            QImage image(view.geometry().size().toSize(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            view.paint(painter, PaintContext{.zoom = 1.0,
                                             .editing = false,
                                             .graphics = nullptr,
                                             .screens = nullptr,
                                             .depth = 0,
                                             .alarms = &alarms});
            painter.end();
            // HMI_ALARMVIEW_SCREENSHOT=<folder> keeps the pictures.
            if (const QString folder = qEnvironmentVariable("HMI_ALARMVIEW_SCREENSHOT");
                !folder.isEmpty())
                QVERIFY(image.save(QStringLiteral("%1/alarmview_%2.png").arg(folder).arg(int(b))));
        }
    }
};

QTEST_MAIN(AlarmEngineTest)
#include "AlarmEngineTest.moc"
