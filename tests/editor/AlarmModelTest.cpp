// The HMI alarms editor: discrete and analog alarm tables, alarm classes,
// undo, and the rules for predefined classes.

#include "editor/models/AlarmModel.h"

#include "core/model/Project.h"
#include "editor/app/AppContext.h"
#include "editor/app/SampleProject.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using namespace Qt::StringLiterals;

class AlarmModelTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::unique_ptr<AppContext> m_app;

    [[nodiscard]] AlarmModel& model() const { return m_app->alarms(); }
    [[nodiscard]] const AlarmConfiguration& alarms() const { return m_app->project().alarms(); }
    [[nodiscard]] QVariant cell(int row, int column, int role = Qt::DisplayRole) const
    {
        return model().data(model().index(row, column), role);
    }

private Q_SLOTS:
    void init()
    {
        m_app = std::make_unique<AppContext>(
            makeSampleProject(), AppContext::Paths{.settingsFile = m_dir.filePath(u"s.ini"_s),
                                                   .autosaveFolder = m_dir.filePath(u"a"_s)});
    }
    void cleanup() { m_app.reset(); }

    void discreteAlarms()
    {
        model().setPage(AlarmModel::DiscretePage);
        const int before = model().rowCount();
        const int alarmsBefore = int(alarms().alarms.size());
        const int index = m_app->undoStack().index();
        const int row = model().addRow();
        QCOMPARE(model().rowCount(), before + 1);
        const Alarm& added = alarms().alarms.back();
        QCOMPARE(added.kind, Alarm::Kind::Discrete);
        QCOMPARE(added.alarmClass, u"Alarm"_s);
        QCOMPARE(cell(row, AlarmModel::IdColumn).toInt(), added.id);

        // Pickers: class, trigger tag, edge; numbers: bit.
        QCOMPARE(cell(row, AlarmModel::ClassColumn, AlarmModel::EditorRole).toString(), u"class"_s);
        QVERIFY(model().setCell(row, AlarmModel::ClassColumn,
                                model().classNames().indexOf(u"Warning"_s)));
        QVERIFY(model().setCell(row, AlarmModel::TagColumn,
                                model().tagNames().indexOf(u"HighLevel"_s)));
        QVERIFY(model().setCell(row, AlarmModel::BitColumn, u"3"_s));
        QVERIFY(model().setCell(row, AlarmModel::EdgeColumn, 1));
        QVERIFY(model().setCell(row, AlarmModel::TextColumn, u"Level {Level} %"_s));
        const Alarm& edited = alarms().alarms.back();
        QCOMPARE(edited.alarmClass, u"Warning"_s);
        QCOMPARE(edited.tag, u"HighLevel"_s);
        QCOMPARE(edited.bit, 3);
        QVERIFY(edited.fallingEdge);
        QCOMPARE(cell(row, AlarmModel::EdgeColumn).toString(), u"On falling edge"_s);

        // Refused, with a reason: a bit past 63, a used number.
        QSignalSpy problems(&model(), &AlarmModel::problem);
        QVERIFY(!model().setCell(row, AlarmModel::BitColumn, u"64"_s));
        if (alarmsBefore > 0)
        {
            QVERIFY(!model().setCell(row, AlarmModel::IdColumn,
                                     QString::number(alarms().alarms.front().id)));
            QCOMPARE(problems.count(), 2);
        }
        // Each edit was one step.
        QCOMPARE(m_app->undoStack().index(), index + 6);
        for (int i = 0; i < 6; ++i)
            m_app->undoStack().undo();
        QCOMPARE(int(alarms().alarms.size()), alarmsBefore);
        QCOMPARE(model().rowCount(), before);
    }

    void analogAlarms()
    {
        model().setPage(AlarmModel::AnalogPage);
        QCOMPARE(model().columnWidth(AlarmModel::BitColumn, 1000, 10), 0.0); // discrete only
        QVERIFY(model().columnWidth(AlarmModel::LimitColumn, 1000, 10) > 0);
        const int row = model().addRow();
        QVERIFY(model().setCell(row, AlarmModel::ModeColumn, int(Alarm::Mode::LessOrEqual)));
        QVERIFY(model().setCell(row, AlarmModel::LimitColumn, u"12.5"_s));
        QVERIFY(!model().setCell(row, AlarmModel::LimitColumn, u"lots"_s));
        const Alarm& alarm = alarms().alarms.back();
        QCOMPARE(alarm.kind, Alarm::Kind::Analog);
        QCOMPARE(alarm.mode, Alarm::Mode::LessOrEqual);
        QCOMPARE(alarm.limit, 12.5);
        QCOMPARE(cell(row, AlarmModel::ModeColumn).toString(), u"Less or equal"_s);
        model().removeRow(row);
        QVERIFY(!alarms().findAlarm(alarm.id));
    }

    void alarmClasses()
    {
        model().setPage(AlarmModel::ClassesPage);
        QCOMPARE(model().rowCount(), int(alarms().classes.size()));
        QCOMPARE(model().columnCount(), int(AlarmModel::ClassColumnCount));
        // Predefined: name and state machine fixed; colours and priority not.
        QCOMPARE(cell(0, AlarmModel::ClassNameColumn, AlarmModel::EditorRole).toString(),
                 u"readonly"_s);
        QVERIFY(model().setCell(0, AlarmModel::PriorityColumn, u"16"_s));
        QCOMPARE(alarms().classes[0].priority, 16);
        QVERIFY(model().setCell(0, AlarmModel::ClassColorColumn, u"#123456"_s)); // background
        QCOMPARE(alarms().classes[0].colors[0].background, QColor(u"#123456"_s));
        QCOMPARE(cell(0, AlarmModel::ClassColorColumn, AlarmModel::SwatchRole).value<QColor>(),
                 QColor(u"#123456"_s));
        QVERIFY(model().setCell(0, AlarmModel::ClassColorColumn + 2, 0)); // no flashing
        QVERIFY(!alarms().classes[0].colors[0].flashing);
        QVERIFY(!model().setCell(0, AlarmModel::ClassColorColumn, u"not a colour"_s));
        QSignalSpy problems(&model(), &AlarmModel::problem);
        model().removeRow(0);
        QCOMPARE(problems.count(), 1); // predefined
        QVERIFY(alarms().findClass(u"Alarm"_s));

        // A user class: renamed with the alarms that use it; deleted only
        // when unused.
        const int row = model().addRow();
        QCOMPARE(alarms().classes.back().name, u"Class_1"_s);
        QCOMPARE(cell(row, AlarmModel::ClassNameColumn, AlarmModel::EditorRole).toString(),
                 u"text"_s);
        QVERIFY(model().setCell(row, AlarmModel::StateMachineColumn,
                                int(AlarmStateMachine::AcknowledgmentAndConfirmation)));
        model().setPage(AlarmModel::DiscretePage);
        const int alarmRow = model().addRow();
        QVERIFY(model().setCell(alarmRow, AlarmModel::ClassColumn,
                                model().classNames().indexOf(u"Class_1"_s)));
        model().setPage(AlarmModel::ClassesPage);
        QVERIFY(model().setCell(row, AlarmModel::ClassNameColumn, u"Maintenance"_s));
        QCOMPARE(alarms().alarms.back().alarmClass, u"Maintenance"_s);
        model().removeRow(row);
        QCOMPARE(problems.count(), 2); // used
        // A predefined class cannot be renamed.
        QVERIFY(!model().setCell(0, AlarmModel::ClassNameColumn, u"Alarms"_s));
        QVERIFY(alarms().findClass(u"Alarm"_s));
    }
};

QTEST_MAIN(AlarmModelTest)
#include "AlarmModelTest.moc"
