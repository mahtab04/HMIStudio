#include "editor/models/LayerModel.h"

#include "core/model/GroupObject.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Rectangle.h"
#include "core/property/Prop.h"
#include "editor/commands/GroupObjectsCommand.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QAbstractItemModelTester>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using namespace Qt::StringLiterals;

class LayerModelTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    QUndoStack m_stack;
    SelectionModel m_selection;
    InputSettings m_input;
    std::unique_ptr<ToolContext> m_context;
    std::unique_ptr<LayerModel> m_model;
    ObjectId m_a; // bottom
    ObjectId m_b;
    ObjectId m_c; // top

    [[nodiscard]] QVariant at(int row, int role) const
    {
        return m_model->data(m_model->index(row), role);
    }

    [[nodiscard]] std::vector<ObjectId> zOrder() const
    {
        std::vector<ObjectId> ids;
        for (const auto& object : m_screen->objects())
            ids.push_back(object->id());
        return ids;
    }

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(u"S"_s);
        builder.add(Rectangle::kTypeId, {0, 0, 10, 10}).with(Prop::Name, u"Base"_s);
        m_a = builder.lastId();
        builder.add(Ellipse::kTypeId, {20, 0, 10, 10});
        m_b = builder.lastId();
        builder.add(Rectangle::kTypeId, {40, 0, 10, 10}).with(Prop::Name, u"Top"_s);
        m_c = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());
        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_model = std::make_unique<LayerModel>(*m_context);
        new QAbstractItemModelTester(
            m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest, m_model.get());
    }

    void cleanup()
    {
        m_model.reset();
        m_stack.clear();
        m_context.reset();
        m_project.reset();
    }

    void topmostFirstWithNames()
    {
        QCOMPARE(m_model->rowCount(), 3);
        QCOMPARE(at(0, LayerModel::NameRole).toString(), u"Top"_s);
        QCOMPARE(at(1, LayerModel::NameRole).toString(), u"Ellipse"_s); // no name: type
        QCOMPARE(at(2, LayerModel::NameRole).toString(), u"Base"_s);
        QVERIFY(at(0, LayerModel::ObjectVisibleRole).toBool());
    }

    void selectingSyncsWithTheCanvas()
    {
        m_model->select(1, false);
        QCOMPARE(m_selection.ids(), std::vector{m_b});
        QVERIFY(at(1, LayerModel::SelectedRole).toBool());
        m_model->select(0, true);
        QCOMPARE(m_selection.ids(), (std::vector{m_b, m_c}));
        m_selection.clear();
        QVERIFY(!at(1, LayerModel::SelectedRole).toBool());
    }

    void renameHideLockAreUndoable()
    {
        m_model->rename(1, u"  Pump  "_s);
        QCOMPARE(m_screen->object(m_b)->name(), u"Pump"_s);
        QCOMPARE(at(1, LayerModel::NameRole).toString(), u"Pump"_s);
        m_model->setObjectVisible(1, false);
        m_model->setLocked(1, true);
        QVERIFY(!m_screen->object(m_b)->isVisible());
        QVERIFY(m_screen->object(m_b)->isLocked());
        QVERIFY(at(1, LayerModel::LockedRole).toBool());
        QCOMPARE(m_stack.count(), 3);
        m_stack.undo();
        m_stack.undo();
        m_stack.undo();
        QVERIFY(m_screen->object(m_b)->isVisible());
        QCOMPARE(at(1, LayerModel::NameRole).toString(), u"Ellipse"_s);
    }

    void dragReorders()
    {
        // Top layer dragged two rows down: to the back.
        QVERIFY(m_model->moveBy(0, 2));
        QCOMPARE(zOrder(), (std::vector{m_c, m_a, m_b}));
        QCOMPARE(at(2, LayerModel::NameRole).toString(), u"Top"_s);
        // Past the end is clamped; zero does nothing.
        QVERIFY(m_model->moveBy(2, -10));
        QCOMPARE(zOrder(), (std::vector{m_a, m_b, m_c}));
        QVERIFY(!m_model->moveBy(1, 0));
        QCOMPARE(m_stack.count(), 2);
        m_stack.undo();
        QCOMPARE(zOrder(), (std::vector{m_c, m_a, m_b}));
    }

    void groupsExpandToShowChildren()
    {
        auto* group = new GroupObjectsCommand(*m_project, m_screen->id(), {m_a, m_b});
        const ObjectId groupId = group->groupId();
        m_stack.push(group);
        QCOMPARE(m_model->rowCount(), 2);
        QVERIFY(at(1, LayerModel::HasChildrenRole).toBool());
        QVERIFY(!at(1, LayerModel::ExpandedRole).toBool());

        m_model->toggleExpanded(1);
        QCOMPARE(m_model->rowCount(), 4);
        QCOMPARE(at(2, LayerModel::DepthRole).toInt(), 1);
        QCOMPARE(at(2, LayerModel::NameRole).toString(), u"Ellipse"_s); // top child first
        QVERIFY(!at(2, LayerModel::EditableRole).toBool());

        // A child row selects its group; children cannot be edited alone or
        // dragged.
        m_model->select(3, false);
        QCOMPARE(m_selection.ids(), std::vector{groupId});
        m_model->rename(3, u"x"_s);
        QVERIFY(!m_model->moveBy(3, -3));
        QCOMPARE(m_stack.count(), 1);

        m_model->toggleExpanded(1);
        QCOMPARE(m_model->rowCount(), 2);
    }
};

QTEST_MAIN(LayerModelTest)
#include "LayerModelTest.moc"
