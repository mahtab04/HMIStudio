#include "editor/models/PropertyModel.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"
#include "core/property/Prop.h"
#include "editor/models/SelectionModel.h"
#include "editor/tools/ToolContext.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>

#include <memory>

using namespace hmi;
using namespace Qt::StringLiterals;

class PropertyModelTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Project> m_project;
    Screen* m_screen = nullptr;
    QUndoStack m_stack;
    SelectionModel m_selection;
    InputSettings m_input;
    std::unique_ptr<ToolContext> m_context;
    std::unique_ptr<PropertyModel> m_model;
    ObjectId m_rect;
    ObjectId m_ellipse;
    ObjectId m_line;
    ObjectId m_text;

    [[nodiscard]] int rowOf(const QString& key) const
    {
        for (int i = 0; i < m_model->rowCount(); ++i)
        {
            if (m_model->data(m_model->index(i), PropertyModel::KeyRole).toString() == key)
                return i;
        }
        return -1;
    }

    [[nodiscard]] QVariant value(const QString& key, int role = PropertyModel::ValueRole) const
    {
        return m_model->data(m_model->index(rowOf(key)), role);
    }

private Q_SLOTS:
    void init()
    {
        ScreenBuilder builder(u"S"_s);
        builder.add(Rectangle::kTypeId, {10, 10, 100, 50}).with(Prop::FillColor, QColor(Qt::red));
        m_rect = builder.lastId();
        builder.add(Ellipse::kTypeId, {200, 10, 80, 80});
        m_ellipse = builder.lastId();
        builder.add(Line::kTypeId, {10, 200, 100, 50});
        m_line = builder.lastId();
        builder.add(TextBox::kTypeId, {200, 200, 120, 40});
        m_text = builder.lastId();
        m_project = std::make_unique<Project>();
        m_screen = *m_project->insertScreen(builder.build());

        m_selection.clear();
        m_context = std::make_unique<ToolContext>(*m_project, m_stack, m_selection, m_input);
        m_context->setScreenId(m_screen->id());
        m_model = std::make_unique<PropertyModel>(*m_context);
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

    void showsScreenSettingsWithoutSelection()
    {
        QCOMPARE(m_model->rowCount(), 4);
        QCOMPARE(value(u"name"_s).toString(), u"S"_s);
        QCOMPARE(value(u"width"_s).toDouble(), Screen::kDefaultSize.width());
        QCOMPARE(m_model->title(), u"Screen S"_s);

        QVERIFY(m_model->setValue(rowOf(u"width"_s), u"1024"_s));
        QCOMPARE(m_screen->size().width(), 1024.0);
        QVERIFY(!m_model->setValue(rowOf(u"height"_s), 0));
        QVERIFY(!m_model->setValue(rowOf(u"name"_s), u"  "_s));
        QCOMPARE(m_stack.count(), 1);
        m_stack.undo();
        QCOMPARE(m_screen->size().width(), Screen::kDefaultSize.width());
        QCOMPARE(value(u"width"_s).toDouble(), Screen::kDefaultSize.width());
    }

    void singleObjectRowsFollowDescriptors()
    {
        m_selection.selectOnly(m_rect);
        QCOMPARE(m_model->title(), u"Rectangle"_s);
        QVERIFY(rowOf(u"fillColor"_s) >= 0);
        QCOMPARE(value(u"fillColor"_s).value<QColor>(), QColor(Qt::red));
        QCOMPARE(value(u"fillColor"_s, PropertyModel::EditorRole).toString(), u"color"_s);
        QCOMPARE(value(u"x"_s, PropertyModel::EditorRole).toString(), u"number"_s);
        QCOMPARE(value(u"visible"_s, PropertyModel::EditorRole).toString(), u"bool"_s);
        QCOMPARE(value(u"lineWidth"_s, PropertyModel::MaximumRole).toDouble(), 50.0);

        // Rows of one category are together, so the panel can use sections.
        QStringList seen;
        QString previous;
        for (int i = 0; i < m_model->rowCount(); ++i)
        {
            const QString category =
                m_model->data(m_model->index(i), PropertyModel::CategoryRole).toString();
            if (category != previous)
            {
                QVERIFY2(!seen.contains(category), qPrintable(category));
                seen << category;
                previous = category;
            }
        }
    }

    void editsPushCommandsAndFollowUndo()
    {
        m_selection.selectOnly(m_rect);
        QSignalSpy changed(m_model.get(), &QAbstractItemModel::dataChanged);
        QVERIFY(m_model->setValue(rowOf(u"x"_s), u"42.5"_s));
        QCOMPARE(m_screen->object(m_rect)->geometry().x(), 42.5);
        QVERIFY(!changed.isEmpty());
        QVERIFY(m_model->setValue(rowOf(u"fillColor"_s), u"#00ff00"_s));
        QCOMPARE(m_stack.count(), 2);
        m_stack.undo();
        QCOMPARE(value(u"fillColor"_s).value<QColor>(), QColor(Qt::red));
        // Setting the same value again adds no step.
        QVERIFY(m_model->setValue(rowOf(u"x"_s), 42.5));
        QCOMPARE(m_stack.index(), 1);
    }

    void rejectsInvalidValues()
    {
        m_selection.selectOnly(m_rect);
        QSignalSpy rejected(m_model.get(), &PropertyModel::editRejected);
        QVERIFY(!m_model->setValue(rowOf(u"width"_s), u"wide"_s));
        QVERIFY(!m_model->setValue(rowOf(u"lineWidth"_s), 500));
        QVERIFY(!m_model->setValue(rowOf(u"fillColor"_s), u"no colour"_s));
        QCOMPARE(rejected.count(), 3);
        QCOMPARE(m_stack.count(), 0);
    }

    void multiSelectionSharesAndMixes()
    {
        m_selection.set({m_rect, m_ellipse, m_text});
        QCOMPARE(m_model->title(), u"3 objects"_s);
        // Fill colour is shared by all three; corner radius only by the
        // rectangle, text only by the text box.
        QVERIFY(rowOf(u"fillColor"_s) >= 0);
        QCOMPARE(rowOf(u"cornerRadius"_s), -1);
        QCOMPARE(rowOf(u"text"_s), -1);
        QVERIFY(value(u"fillColor"_s, PropertyModel::MixedRole).toBool());
        QVERIFY(value(u"width"_s, PropertyModel::MixedRole).toBool()); // 100, 80, 120

        QVERIFY(m_model->setValue(rowOf(u"fillColor"_s), QColor(Qt::blue)));
        QVERIFY(!value(u"fillColor"_s, PropertyModel::MixedRole).toBool());
        QCOMPARE(m_stack.count(), 1);
        for (const ObjectId id : {m_rect, m_ellipse, m_text})
            QCOMPARE(m_screen->object(id)->value<QColor>(Prop::FillColor), QColor(Qt::blue));
        m_stack.undo();
        QCOMPARE(m_screen->object(m_rect)->value<QColor>(Prop::FillColor), QColor(Qt::red));
    }

    void heldStepperIsOneUndoStep()
    {
        m_selection.selectOnly(m_rect);
        const int row = rowOf(u"x"_s);
        m_model->beginEdit();
        for (int i = 0; i < 5; ++i)
            QVERIFY(m_model->stepValue(row, 1));
        m_model->endEdit();
        QCOMPARE(m_screen->object(m_rect)->geometry().x(), 15.0);
        QCOMPARE(m_stack.count(), 1);

        // Separate presses are separate steps.
        QVERIFY(m_model->stepValue(row, -1));
        QVERIFY(m_model->stepValue(row, -1));
        QCOMPARE(m_stack.count(), 3);
        m_stack.undo();
        m_stack.undo();
        m_stack.undo();
        QCOMPARE(m_screen->object(m_rect)->geometry().x(), 10.0);
    }

    void stepStopsAtLimits()
    {
        m_selection.selectOnly(m_rect);
        QVERIFY(m_model->setValue(rowOf(u"lineWidth"_s), 49.5));
        QVERIFY(m_model->stepValue(rowOf(u"lineWidth"_s), 5));
        QCOMPARE(m_screen->object(m_rect)->value<double>(Prop::LineWidth), 50.0);
    }

    void enumsUseOptionIndices()
    {
        m_selection.selectOnly(m_line);
        const QStringList options =
            value(u"lineDirection"_s, PropertyModel::OptionsRole).toStringList();
        QCOMPARE(options.size(), 2);
        QCOMPARE(value(u"lineDirection"_s).toInt(), 0);
        QVERIFY(m_model->setValue(rowOf(u"lineDirection"_s), 1));
        QCOMPARE(m_screen->object(m_line)->value<EnumValue>(Prop::LineDirection).value,
                 int(Line::Rising));
        QVERIFY(!m_model->setValue(rowOf(u"lineDirection"_s), 7));
    }

    void fontParts()
    {
        m_selection.selectOnly(m_text);
        const int row = rowOf(u"font"_s);
        QCOMPARE(value(u"font"_s, PropertyModel::EditorRole).toString(), u"font"_s);
        QVERIFY(m_model->setFontPixelSize(row, 30));
        QVERIFY(m_model->setFontBold(row, true));
        QVERIFY(m_model->setFontItalic(row, true));
        const QFont font = m_screen->object(m_text)->value<QFont>(Prop::Font);
        QCOMPARE(font.pixelSize(), 30);
        QVERIFY(font.bold() && font.italic());
        QVERIFY(!m_model->setFontPixelSize(row, 0));
        QCOMPARE(m_stack.count(), 3);
    }

    void followsSelection()
    {
        m_selection.selectOnly(m_text);
        QVERIFY(rowOf(u"text"_s) >= 0);
        m_selection.clear();
        QCOMPARE(rowOf(u"text"_s), -1);
        QVERIFY(rowOf(u"background"_s) >= 0);
    }
};

QTEST_MAIN(PropertyModelTest)
#include "PropertyModelTest.moc"
