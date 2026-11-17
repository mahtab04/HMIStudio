// The Tags panel model, the property panel's bindings and play mode, on the
// sample project (whose screen is bound to simulated tags).

#include "editor/models/TagModel.h"

#include "comm/BindingEvaluator.h"
#include "comm/TagEngine.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/objects/Tank.h"
#include "core/property/Prop.h"
#include "core/tags/TagUsage.h"
#include "editor/app/AppContext.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/SimulationController.h"
#include "editor/models/PropertyModel.h"
#include "editor/models/SelectionModel.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace hmi;
using namespace Qt::StringLiterals;

class TagModelTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::unique_ptr<AppContext> m_app;

    [[nodiscard]] TagModel& tags() const { return m_app->tags(); }
    [[nodiscard]] Project& project() const { return m_app->project(); }

    [[nodiscard]] int tagRow(const QString& name) const
    {
        for (int i = 0; i < tags().rowCount(); ++i)
        {
            if (tags().data(tags().index(i, TagModel::NameColumn), Qt::DisplayRole) == name)
                return i;
        }
        return -1;
    }

    [[nodiscard]] QVariant cell(const QString& name, int column) const
    {
        return tags().data(tags().index(tagRow(name), column), Qt::DisplayRole);
    }

    [[nodiscard]] std::size_t uses(const QString& tag) const
    {
        return TagUsage::find(project(), tag).size();
    }

    [[nodiscard]] HmiObject* tank() const
    {
        for (const auto& object : project().screens().front()->objects())
        {
            if (object->typeId() == Tank::kTypeId)
                return object.get();
        }
        return nullptr;
    }

    [[nodiscard]] int propertyRow(const QString& key) const
    {
        PropertyModel& model = m_app->properties();
        for (int i = 0; i < model.rowCount(); ++i)
        {
            if (model.data(model.index(i), PropertyModel::KeyRole).toString() == key)
                return i;
        }
        return -1;
    }

    [[nodiscard]] QString bindingText(const QString& key) const
    {
        PropertyModel& model = m_app->properties();
        return model.data(model.index(propertyRow(key)), PropertyModel::BindingRole).toString();
    }

private Q_SLOTS:
    void init()
    {
        auto sample = makeSampleProject();
        QVERIFY(sample->setSimulationProfile({})); // isolate the generic tag model and generators
        m_app = std::make_unique<AppContext>(
            std::move(sample), AppContext::Paths{.settingsFile = m_dir.filePath(u"s.ini"_s),
                                                 .autosaveFolder = m_dir.filePath(u"auto"_s)});
    }
    void cleanup() { m_app.reset(); }

    void sampleScreenIsBound()
    {
        QCOMPARE(project().tags().size(), std::size_t(10));
        QVERIFY(uses(u"Level"_s) >= 4);
        QVERIFY(uses(u"PumpRunning"_s) >= 3);
        QVERIFY(tank()->binding(Prop::Value));
    }

    void buttonTargetPickerAndRenameUndo()
    {
        auto* screen = project().screens()[0].get();
        const auto button = std::ranges::find_if(
            screen->objects(),
            [](const auto& object)
            {
                const auto target = object->property(Prop::ActionTag);
                return target && std::get<TagRef>(*target).tagName == u"RunCommand";
            });
        QVERIFY(button != screen->objects().end());
        m_app->selection().selectOnly((*button)->id());
        const int row = propertyRow(u"actionTag"_s);
        QVERIFY(row >= 0);
        QCOMPARE(
            m_app->properties().data(m_app->properties().index(row), PropertyModel::EditorRole),
            QVariant(u"tagReference"_s));
        QCOMPARE(m_app->properties()
                     .data(m_app->properties().index(row), PropertyModel::OptionsRole)
                     .toStringList()
                     .size(),
                 10);
        QCOMPARE(uses(u"RunCommand"_s), std::size_t(2));
        QVERIFY(tags().setCell(tagRow(u"RunCommand"_s), TagModel::NameColumn, u"RequestRun"_s));
        QCOMPARE((*button)->value<TagRef>(Prop::ActionTag).tagName, u"RequestRun"_s);
        QCOMPARE(uses(u"RequestRun"_s), std::size_t(2));
        m_app->undoStack().undo();
        QCOMPARE((*button)->value<TagRef>(Prop::ActionTag).tagName, u"RunCommand"_s);
        QCOMPARE(uses(u"RunCommand"_s), std::size_t(2));
        m_app->undoStack().redo();
        QCOMPARE((*button)->value<TagRef>(Prop::ActionTag).tagName, u"RequestRun"_s);
    }

    void tableAndFilter()
    {
        QAbstractItemModelTester tester(&tags(),
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(tags().rowCount(), 10);
        QCOMPARE(tags().columnCount(), int(TagModel::ColumnCount));
        QCOMPARE(cell(u"Level"_s, TagModel::UnitColumn).toString(), u"%"_s);
        QCOMPARE(cell(u"Level"_s, TagModel::SimulationColumn).toString(), u"None"_s);
        QCOMPARE(tags().headerData(TagModel::PeriodColumn, Qt::Horizontal, Qt::DisplayRole),
                 QVariant(u"Period (s)"_s));

        tags().setFilterText(u"PRES"_s); // name, case-insensitive
        QCOMPARE(tags().rowCount(), 1);
        tags().setFilterText(u"temperature"_s); // also the comment
        QCOMPARE(tags().rowCount(), 1);
        QCOMPARE(tags().tagCount(), 10);
        tags().setFilterText({});
        QCOMPARE(tags().rowCount(), 10);
        QCOMPARE(tags().namesMatching(u"E"_s).size(), 6); // not Flow or PumpRunning
    }

    void editingIsUndoable()
    {
        QUndoStack& stack = m_app->undoStack();
        const int row = tags().addTag();
        QCOMPARE(tags().rowCount(), 11);
        QCOMPARE(tags().data(tags().index(row, TagModel::NameColumn), Qt::DisplayRole),
                 QVariant(u"Tag_1"_s));
        // The type of the last tag (Temperature, Real), then the next number.
        QCOMPARE(project().tags().find(u"Tag_1"_s)->type, Tag::Type::Real);
        QVERIFY(tags().setCell(row, TagModel::UnitColumn, u"rpm"_s));
        QVERIFY(tags().setCell(row, TagModel::TypeColumn, 1)); // integer
        QCOMPARE(project().tags().find(u"Tag_1"_s)->type, Tag::Type::Int);

        QSignalSpy problems(&tags(), &TagModel::problem);
        QVERIFY(!tags().setCell(row, TagModel::MaximumColumn, u"lots"_s));
        QVERIFY(!tags().setCell(row, TagModel::PeriodColumn, u"0"_s));
        QVERIFY(!tags().setCell(row, TagModel::NameColumn, u"level"_s)); // taken
        QCOMPARE(problems.count(), 3);

        stack.undo();
        stack.undo();
        QCOMPARE(project().tags().find(u"Tag_1"_s)->unit, QString());
        stack.undo();
        QCOMPARE(tags().rowCount(), 10);
    }

    void renamingUpdatesBindings()
    {
        const std::size_t before = uses(u"Level"_s);
        const int index = m_app->undoStack().index();
        QVERIFY(tags().setCell(tagRow(u"Level"_s), TagModel::NameColumn, u"TankLevel"_s));
        QCOMPARE(uses(u"TankLevel"_s), before);
        QCOMPARE(uses(u"Level"_s), std::size_t(0));
        QCOMPARE(tank()->binding(Prop::Value)->tag, u"TankLevel"_s);
        QCOMPARE(m_app->undoStack().index(), index + 1); // one step

        m_app->undoStack().undo();
        QCOMPARE(uses(u"Level"_s), before);
        QVERIFY(project().tags().find(u"Level"_s));
        QVERIFY(!project().tags().find(u"TankLevel"_s));
    }

    void addingSeveral()
    {
        const int index = m_app->undoStack().index();
        const int row = tags().addTags(u"Counter"_s, 1, 5); // Int
        QCOMPARE(tags().rowCount(), 15);
        QCOMPARE(tags().data(tags().index(row, TagModel::NameColumn), Qt::DisplayRole),
                 QVariant(u"Counter_1"_s));
        QCOMPARE(project().tags().find(u"Counter_5"_s)->type, Tag::Type::Int);
        QCOMPARE(cell(u"Counter_3"_s, TagModel::TypeColumn).toString(), u"Int"_s);
        QCOMPARE(m_app->undoStack().index(), index + 1); // one step

        // Names in use are skipped.
        tags().addTags(u"Counter"_s, 0, 2);
        QVERIFY(project().tags().find(u"Counter_7"_s));
        QCOMPARE(cell(u"Counter_7"_s, TagModel::MinimumColumn).toString(), QString()); // a flag
        m_app->undoStack().undo();
        m_app->undoStack().undo();
        QCOMPARE(tags().rowCount(), 10);
    }

    void connectionWorkflowAndUndo()
    {
        TagConnections* connections = tags().connections();
        const int before = m_app->undoStack().index();
        QVERIFY(connections->save(-1, {{u"name"_s, u"LinePLC"_s},
                                       {u"driver"_s, u"s7"_s},
                                       {u"host"_s, u"fixture.invalid"_s}}));
        QCOMPARE(project().connections().size(), std::size_t(1));
        const QString id = project().connections()[0].id;
        const int row = tags().createTag(u"TankLevel"_s, 2, u"Tank"_s, 1, u"DB1.DBD4"_s);
        QVERIFY(row >= 0);
        QCOMPARE(project().tags().find(u"TankLevel"_s)->connection, id);
        QCOMPARE(project().tags().find(u"TankLevel"_s)->table, u"Tank"_s);
        QCOMPARE(cell(u"TankLevel"_s, TagModel::ConnectionColumn), QVariant(u"LinePLC"_s));
        QVERIFY(tags().setCell(row, TagModel::AcquisitionColumn, u"1000"_s));
        QVERIFY(!tags().setCell(row, TagModel::AcquisitionColumn, u"0"_s));
        QVERIFY(!connections->remove(0));
        QVERIFY(connections->save(0, {{u"name"_s, u"RenamedPLC"_s}}));
        QCOMPARE(project().tags().find(u"TankLevel"_s)->connection, id);
        QCOMPARE(cell(u"TankLevel"_s, TagModel::ConnectionColumn), QVariant(u"RenamedPLC"_s));
        tags().setTableFilter(u"Tank"_s);
        QCOMPARE(tags().rowCount(), 1);
        QVERIFY(tags().columnWidth(TagModel::ConnectionColumn, 1200, 50) > 0);
        QCOMPARE(tags().columnWidth(TagModel::SimulationColumn, 1200, 50), 0.0);
        tags().setSimulationSettings(true);
        QCOMPARE(tags().columnWidth(TagModel::ConnectionColumn, 1200, 50), 0.0);
        QVERIFY(tags().columnWidth(TagModel::SimulationColumn, 1200, 50) > 0);
        tags().setTableFilter({});
        while (m_app->undoStack().index() > before)
            m_app->undoStack().undo();
        QVERIFY(project().connections().empty());
        QVERIFY(!project().tags().find(u"TankLevel"_s));
    }

    void externalTagCreationValidationAndCsv()
    {
        auto* connections = tags().connections();
        QVERIFY(connections->save(
            -1,
            {{u"name"_s, u"AB"_s}, {u"driver"_s, u"ab-eip"_s}, {u"host"_s, u"fixture.invalid"_s}}));
        QVERIFY(tags().createTag(u"External"_s, 1, u"Process"_s, 1, u"Motor.Counter"_s) >= 0);
        QCOMPARE(tags().createTag(u"External"_s, 1, u"Process"_s, 1, u"Other"_s), -1);
        QCOMPARE(tags().createTag(u"Bad"_s, 1, u"Process"_s, 1, u"A&gateway=other"_s), -1);
        const QString id = project().connections()[0].id;
        const QUrl file = QUrl::fromLocalFile(m_dir.filePath(u"external.csv"_s));
        QVERIFY(tags().exportCsv(file));
        QFile csv(file.toLocalFile());
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QByteArray bytes = csv.readAll();
        QVERIFY(bytes.contains(",AB,Motor.Counter,"));
        csv.close();
        tags().removeTag(tagRow(u"External"_s));
        QVERIFY(tags().importCsv(file) > 0);
        QCOMPARE(project().tags().find(u"External"_s)->connection, id);
        QCOMPARE(project().tags().find(u"External"_s)->address, u"Motor.Counter"_s);
        QCOMPARE(project().tags().find(u"External"_s)->table, u"Process"_s);
    }

    void processTagInProperties()
    {
        // The process value's tag comes first, as a picker of the tags.
        PropertyModel& model = m_app->properties();
        m_app->selection().selectOnly(tank()->id());
        const QModelIndex first = model.index(0);
        QCOMPARE(model.data(first, PropertyModel::EditorRole).toString(), u"tag"_s);
        QCOMPARE(model.data(first, PropertyModel::CategoryRole).toString(), u"Process"_s);
        QCOMPARE(model.data(first, PropertyModel::ValueRole).toString(), u"Level"_s);
        QCOMPARE(model.data(first, PropertyModel::OptionsRole).toStringList().size(), 10);

        // Picking another tag keeps the transform; None removes the binding.
        QVERIFY(model.setValue(0, u"Flow"_s));
        QCOMPARE(tank()->binding(Prop::Value)->tag, u"Flow"_s);
        QCOMPARE(model.data(first, PropertyModel::TextRole).toString(), u"Flow"_s);
        QVERIFY(model.setValue(0, QString()));
        QVERIFY(!tank()->binding(Prop::Value));
        QCOMPARE(model.data(first, PropertyModel::TextRole).toString(), u"None"_s);
        m_app->undoStack().undo();
        m_app->undoStack().undo();
        QCOMPARE(tank()->binding(Prop::Value)->tag, u"Level"_s);

        // Objects without a process value have no such row.
        for (const auto& object : project().screens().front()->objects())
        {
            if (object->name() == u"Header")
                m_app->selection().selectOnly(object->id());
        }
        QVERIFY(model.data(model.index(0), PropertyModel::EditorRole).toString() != u"tag");
    }

    void deletingKeepsBindingsAndSaysWhereUsed()
    {
        const int row = tagRow(u"Level"_s);
        QCOMPARE(tags().usageCount(row), int(uses(u"Level"_s)));
        const QStringList places = tags().whereUsed(row);
        QVERIFY(places.contains(u"02 Filling · T-101 · Value"_s));
        tags().removeTag(row);
        QVERIFY(!project().tags().find(u"Level"_s));
        QVERIFY(tank()->binding(Prop::Value)); // stays, shows the tag as missing
        m_app->undoStack().undo();
        QCOMPARE(tags().rowCount(), 10);
        QCOMPARE(tagRow(u"Level"_s), row); // back where it was
    }

    void csvRoundTrip()
    {
        const QUrl file = QUrl::fromLocalFile(m_dir.filePath(u"tags.csv"_s));
        QVERIFY(tags().exportCsv(file));

        // Importing the same file updates; an extra line adds, in one step.
        QFile out(file.toLocalFile());
        QVERIFY(out.open(QIODevice::Append));
        out.write("Speed,Int,rpm,0,3000,Ramp,10,0,Conveyor speed\n");
        out.close();
        QVERIFY(tags().setCell(tagRow(u"Flow"_s), TagModel::UnitColumn, u"m3/h"_s));
        const int index = m_app->undoStack().index();
        QCOMPARE(tags().importCsv(file), 11);
        QCOMPARE(m_app->undoStack().index(), index + 1);
        QCOMPARE(tags().rowCount(), 11);
        QCOMPARE(project().tags().find(u"Flow"_s)->unit, u"L/min"_s);
        QCOMPARE(project().tags().find(u"Speed"_s)->maximum, 3000.0);
        m_app->undoStack().undo();
        QCOMPARE(tags().rowCount(), 10);

        QSignalSpy problems(&tags(), &TagModel::problem);
        QCOMPARE(tags().importCsv(QUrl::fromLocalFile(m_dir.filePath(u"none.csv"_s))), -1);
        QCOMPARE(problems.count(), 1);
    }

    void propertyPanelBindings()
    {
        PropertyModel& model = m_app->properties();
        m_app->selection().selectOnly(tank()->id());
        const int valueRow = propertyRow(u"value"_s);
        QVERIFY(valueRow >= 0);
        QVERIFY(model.data(model.index(valueRow), PropertyModel::BindableRole).toBool());
        QCOMPARE(bindingText(u"value"_s), u"Level"_s);
        QVERIFY(bindingText(u"fillColor"_s).startsWith(u"Level · "_s));
        QCOMPARE(bindingText(u"name"_s), QString());

        // Linear scaling, then undo.
        QVERIFY(model.setBinding(
            valueRow, u"Flow"_s, u"linear"_s,
            {{u"inMin"_s, 0}, {u"inMax"_s, 60}, {u"outMin"_s, 0}, {u"outMax"_s, 100}}));
        QCOMPARE(tank()->binding(Prop::Value)->tag, u"Flow"_s);
        const QVariantMap info = model.bindingInfo(valueRow);
        QCOMPARE(info.value(u"kind"_s).toString(), u"linear"_s);
        QCOMPARE(info.value(u"parameters"_s).toMap().value(u"inMax"_s).toDouble(), 60.0);
        QVERIFY(bindingText(u"value"_s).startsWith(u"Flow · "_s));
        m_app->undoStack().undo();
        QCOMPARE(bindingText(u"value"_s), u"Level"_s);

        model.removeBinding(valueRow);
        QVERIFY(!tank()->binding(Prop::Value));
        m_app->undoStack().undo();
        QVERIFY(tank()->binding(Prop::Value));

        // A missing tag is shown so; refused edits say why.
        QVERIFY(model.setBinding(valueRow, u"Ghost"_s, u"direct"_s, {}));
        QCOMPARE(bindingText(u"value"_s), u"Ghost (missing)"_s);
        QSignalSpy rejected(&model, &PropertyModel::editRejected);
        QVERIFY(!model.setBinding(valueRow, u"Level"_s, u"expression"_s, {{u"text"_s, u"x +"_s}}));
        QVERIFY(!model.setBinding(propertyRow(u"name"_s), u"Level"_s, u"direct"_s, {}));
        QCOMPARE(rejected.count(), 2);

        QSignalSpy requested(&model, &PropertyModel::bindingRequested);
        model.requestBinding(valueRow);
        model.requestBinding(propertyRow(u"name"_s)); // not bindable
        QCOMPARE(requested.count(), 1);
    }

    void rangesAsText()
    {
        PropertyModel& model = m_app->properties();
        const QVariantMap parameters =
            model.rangesParameters(u"0..50 = #2E9B4F\n\n50 .. 80 = 2.5\nelse = high"_s);
        const QVariantList ranges = parameters.value(u"ranges"_s).toList();
        QCOMPARE(ranges.size(), 2);
        QCOMPARE(ranges[1].toMap().value(u"value"_s), QVariant(2.5));
        QCOMPARE(parameters.value(u"default"_s), QVariant(u"high"_s));
        QCOMPARE(model.rangesText(parameters), u"0..50 = #2E9B4F\n50..80 = 2.5\nelse = high"_s);

        QSignalSpy rejected(&model, &PropertyModel::editRejected);
        QVERIFY(model.rangesParameters(u"0-50 = red"_s).isEmpty());
        QCOMPARE(rejected.count(), 1);
    }

    void simulating()
    {
        SimulationController& simulation = m_app->simulation();
        TagEngine& engine = m_app->tagEngine();
        QSignalSpy changed(&simulation, &SimulationController::runningChanged);
        simulation.toggle();
        QVERIFY(simulation.isRunning());
        QVERIFY(engine.isRunning());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(simulation.screenTitle(), u"02 Filling"_s);
        QCOMPARE(simulation.screenSize(), Screen::kDefaultSize);
        QVERIFY(simulation.problems().isEmpty());

        // Typing a value writes it to the tag.
        QVERIFY(tags().setCell(tagRow(u"PumpRunning"_s), TagModel::ValueColumn, u"true"_s));
        QCOMPARE(engine.value(u"PumpRunning"_s), TagValue(true));
        QTRY_COMPARE(cell(u"PumpRunning"_s, TagModel::ValueColumn).toString(), u"true"_s);

        // The simulation drives a copy; the document never changes.
        const Screen* play = simulation.evaluator().playScreen();
        QVERIFY(play && play != project().screens().front().get());
        const double level = tank()->value<double>(Prop::Value);
        QTRY_VERIFY(play->object(tank()->id())->value<double>(Prop::Value) != level);
        QCOMPARE(tank()->value<double>(Prop::Value), level);

        // Editing goes on while it runs, as in TIA Portal; a restart picks
        // the edits up.
        QVERIFY(m_app->properties().rowCount() > 0);
        m_app->selection().selectOnly(tank()->id());
        QVERIFY(m_app->properties().setValue(propertyRow(u"name"_s), u"T-102"_s));
        QCOMPARE(play->object(tank()->id())->name(), u"T-101"_s);
        simulation.restart();
        QVERIFY(simulation.isRunning());
        QCOMPARE(simulation.evaluator().playScreen()->object(tank()->id())->name(), u"T-102"_s);
        m_app->undoStack().undo();
        QCOMPARE(tank()->name(), u"T-101"_s);

        simulation.toggle();
        QVERIFY(!simulation.isRunning());
        QVERIFY(!engine.isRunning());
        QVERIFY(!simulation.evaluator().playScreen());
    }
};

QTEST_MAIN(TagModelTest)
#include "TagModelTest.moc"
