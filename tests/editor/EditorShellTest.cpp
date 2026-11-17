// Loads the real QML shell offscreen. Set HMI_SCREENSHOT=<file.png> to save
// an image of the window for a visual check.

#include "comm/TagEngine.h"
#include "core/model/Project.h"
#include "core/property/Prop.h"
#include "editor/app/AppContext.h"
#include "editor/app/AppSettings.h"
#include "editor/app/DocumentController.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/SimulationController.h"
#include "editor/app/Theme.h"
#include "editor/models/EventModel.h"
#include "editor/models/LayerModel.h"
#include "editor/models/PropertyModel.h"
#include "editor/models/ScreenListModel.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/TagModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"
#include "quick/CanvasItem.h"
#include "quick/RuntimeView.h"
#include "quick/ScreenItem.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(HmiStudio_QuickPlugin)
Q_IMPORT_QML_PLUGIN(HmiStudio_EditorPlugin)

using namespace hmi;

class EditorShellTest : public QObject
{
    Q_OBJECT

    // The runtime simulator's window, while it is open.
    static QQuickWindow* simulatorWindow(QQmlApplicationEngine& engine)
    {
        for (QWindow* window : QGuiApplication::topLevelWindows())
        {
            auto* quick = qobject_cast<QQuickWindow*>(window);
            if (quick && quick != engine.rootObjects().front() &&
                quick->findChild<RuntimeView*>(QStringLiteral("runtimeView")))
                return quick;
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Fusion")); }

    void loadsAndAttachesCanvas()
    {
        // Any warning that mentions a QML file is a failure.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("\\.qml")));

        // Temporary settings and autosave folders: the test must not read or
        // change the user's.
        QTemporaryDir dir;
        AppContext context(makeSampleProject(),
                           {.settingsFile = dir.filePath(QStringLiteral("settings.ini")),
                            .autosaveFolder = dir.filePath(QStringLiteral("autosave"))});
        QGuiApplication::setPalette(context.theme().palette());
        context.theme().setDensity(Theme::Density::Compact);

        QQmlApplicationEngine engine;
        engine.setInitialProperties({
            {QStringLiteral("editor"), QVariant::fromValue(&context.editor())},
            {QStringLiteral("theme"), QVariant::fromValue(&context.theme())},
            {QStringLiteral("toolbox"), QVariant::fromValue(&context.toolbox())},
            {QStringLiteral("toolboxDrag"), QVariant::fromValue(&context.toolboxDrag())},
            {QStringLiteral("document"), QVariant::fromValue(&context.document())},
            {QStringLiteral("settings"), QVariant::fromValue(&context.settings())},
            {QStringLiteral("properties"), QVariant::fromValue(&context.properties())},
            {QStringLiteral("events"), QVariant::fromValue(&context.events())},
            {QStringLiteral("layers"), QVariant::fromValue(&context.layers())},
            {QStringLiteral("screens"), QVariant::fromValue(&context.screens())},
            {QStringLiteral("tags"), QVariant::fromValue(&context.tags())},
            {QStringLiteral("simulation"), QVariant::fromValue(&context.simulation())},
        });
        engine.loadFromModule("HmiStudio.Editor", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);

        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));

        auto* canvas = window->findChild<CanvasItem*>();
        QVERIFY(canvas);
        const Screen& screen = *context.project().screens().front();
        QCOMPARE(canvas->screenItem()->objectItemCount(), int(screen.objectCount()));
        QTRY_VERIFY(canvas->zoom() < 1.0); // fitted into the canvas area
        QVERIFY(context.editor().selectToolActive());

        // The zoom strip under the canvas: presets and a slider over log2(zoom).
        QVERIFY(window->findChild<QQuickItem*>(QStringLiteral("zoomBox")));
        const double fitted = canvas->zoom();
        context.editor().chooseZoom(4); // 100 %
        QCOMPARE(canvas->zoom(), 1.0);
        context.editor().setZoomLevel(1.0);
        QCOMPARE(canvas->zoom(), 2.0);
        context.editor().chooseZoom(0); // fit
        QCOMPARE(canvas->zoom(), fitted);

        // Switching density resizes the controls.
        auto* header = window->property("header").value<QQuickItem*>();
        QVERIFY(header);
        const double compactHeight = header->height();
        context.theme().setDensity(Theme::Density::Comfortable);
        QTRY_VERIFY(header->height() > compactHeight);
        context.theme().setDensity(Theme::Density::Compact);
        QTRY_COMPARE(header->height(), compactHeight);

        // Selecting shows the size label; the touch toolbar appears only in
        // the comfortable (touch) layout.
        auto* sizeLabel = window->findChild<QQuickItem*>(QStringLiteral("sizeLabel"));
        auto* toolbar = window->findChild<QQuickItem*>(QStringLiteral("selectionToolbar"));
        QVERIFY(sizeLabel && toolbar);
        QVERIFY(!sizeLabel->isVisible());
        const auto rectangle = std::ranges::find_if(
            screen.objects(), [](const auto& item)
            { return item->typeId() == ObjectTypeId{"rectangle"} && item->geometry().y() > 100; });
        QVERIFY(rectangle != screen.objects().end());
        const HmiObject& object = **rectangle;
        context.selection().selectOnly(object.id());
        QTRY_VERIFY(sizeLabel->isVisible());
        QCOMPARE(sizeLabel->property("text").toString(),
                 QStringLiteral("%1 × %2")
                     .arg(qRound(object.geometry().width()))
                     .arg(qRound(object.geometry().height())));
        QVERIFY(!toolbar->isVisible());
        context.theme().setDensity(Theme::Density::Comfortable);
        QTRY_VERIFY(toolbar->isVisible());
        // It sits above the selection, clear of it.
        QVERIFY(toolbar->mapToItem(canvas, QPointF(0, toolbar->height())).y() <
                canvas->selectionBounds().top());

        // The properties panel lists the selected object's properties, and
        // there is one tab per screen.
        QTRY_VERIFY(context.properties().title().startsWith(u"Rectangle"));
        QVERIFY(context.properties().rowCount() > 8);
        QCOMPARE(context.screens().rowCount(), int(context.project().screenCount()));

        // A side panel never takes more than 30 % of the window (a size saved
        // in a larger window cannot squeeze the screen), nor less than its
        // usual width.
        {
            auto* properties = window->findChild<QQuickItem*>(QStringLiteral("propertiesPanel"));
            QVERIFY(properties);
            QQmlProperty maximum(properties, QStringLiteral("SplitView.maximumWidth"),
                                 qmlContext(properties));
            QVERIFY(maximum.isValid());
            window->resize(1432, 900);
            QTRY_COMPARE(window->width(), 1432);
            // The QML bindings follow the resize a moment later.
            QTRY_VERIFY(maximum.read().toDouble() <= 1432 * 0.3 + 1);
            QVERIFY(maximum.read().toDouble() >= context.theme().sidePanelWidth());
            QVERIFY(properties->width() <= maximum.read().toDouble());
        }

        // Right-click (or the toolbar's "more" button) opens the menu.
        auto* menu = window->findChild<QObject*>(QStringLiteral("contextMenu"));
        QVERIFY(menu);
        Q_EMIT context.editor().contextMenuRequested(QPointF(100, 100));
        QTRY_VERIFY(menu->property("opened").toBool());
        QMetaObject::invokeMethod(menu, "close");
        QTRY_VERIFY(!menu->property("visible").toBool());

        // The Events tab lists the selected button's actions.
        {
            auto* tabs = window->findChild<QQuickItem*>(QStringLiteral("propertiesTabs"));
            QVERIFY(tabs);
            for (const auto& item : screen.objects())
            {
                if (item->name() == u"Start transfer")
                    context.selection().selectOnly(item->id());
            }
            QVERIFY(context.events().available());
            tabs->setProperty("currentIndex", 1);
            QCOMPARE(context.events().rowCount(), 1);
            if (const QString path = qEnvironmentVariable("HMI_EVENTS_SCREENSHOT"); !path.isEmpty())
            {
                QMetaObject::invokeMethod(menu, "close");
                context.theme().setDensity(Theme::Density::Compact);
                QTest::qWait(300);
                QVERIFY(window->grabWindow().save(path));
            }
            tabs->setProperty("currentIndex", 0);
            context.selection().selectOnly(object.id());
        }

        // The binding dialog lists the tags as they are when it opens.
        {
            const int row = [&]
            {
                for (int i = 0; i < context.properties().rowCount(); ++i)
                {
                    if (context.properties()
                            .data(context.properties().index(i), PropertyModel::BindableRole)
                            .toBool())
                        return i;
                }
                return -1;
            }();
            QVERIFY(row >= 0);
            auto* popup = window->findChild<QObject*>(QStringLiteral("bindingPopup"));
            auto* list = window->findChild<QQuickItem*>(QStringLiteral("bindingTagList"));
            QVERIFY(popup && list);
            context.properties().requestBinding(row);
            QTRY_VERIFY(popup->property("opened").toBool());
            QTRY_COMPARE(list->property("count").toInt(), 10);
            QMetaObject::invokeMethod(popup, "close");
            QTRY_VERIFY(!popup->property("visible").toBool());

            context.tags().removeTag(5);
            context.tags().addTag();
            context.properties().requestBinding(row);
            QTRY_VERIFY(popup->property("opened").toBool());
            QTRY_COMPARE(list->property("count").toInt(), 10);
            const QVariant names = list->property("model");
            QVERIFY(names.toStringList().contains(QStringLiteral("Tag_1")));
            QVERIFY(!names.toStringList().contains(QStringLiteral("Level")));
            QMetaObject::invokeMethod(popup, "close");
            context.undoStack().undo();
            context.undoStack().undo();
        }

        // The HMI tags tab opens the tag table in the work area.
        auto* tagsTab = window->findChild<QQuickItem*>(QStringLiteral("tagsTab"));
        QVERIFY(tagsTab);
        QMetaObject::invokeMethod(tagsTab, "click");
        QTRY_VERIFY(!canvas->isVisible());

        // A click on a cell makes its row current: Delete deletes that tag.
        auto* table = window->findChild<QQuickItem*>(QStringLiteral("tagTable"));
        auto* deleteButton = window->findChild<QQuickItem*>(QStringLiteral("deleteTagButton"));
        QVERIFY(table && deleteButton);
        QTRY_VERIFY(table->isVisible() && table->height() > 0);
        QVERIFY(!deleteButton->isEnabled());
        const auto cellItem = [&](int row, int column)
        {
            QQuickItem* item = nullptr;
            QMetaObject::invokeMethod(table, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, item),
                                      Q_ARG(QModelIndex, context.tags().index(row, column)));
            return item;
        };
        QQuickItem* nameCell = nullptr;
        table->setProperty("contentY", 9 * (context.theme().controlHeight() + 1));
        QTRY_VERIFY((nameCell = cellItem(9, TagModel::NameColumn)) != nullptr);
        if (const QString path = qEnvironmentVariable("HMI_TAGS_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(path));
        }
        QTest::mouseClick(window, Qt::LeftButton, {},
                          nameCell->mapToScene(QPointF(10, nameCell->height() / 2)).toPoint());
        QTRY_VERIFY(deleteButton->isEnabled());
        // Every row's type is a drop-down.
        QQuickItem* typeCell = cellItem(9, TagModel::TypeColumn);
        QVERIFY(typeCell);
        QVERIFY(std::ranges::any_of(
            typeCell->childItems(), [](QQuickItem* child)
            { return child->inherits("QQuickComboBox") && child->isVisible(); }));
        // Temperature is bound on the screen: Delete asks first.
        QMetaObject::invokeMethod(deleteButton, "click");
        auto* deleteDialog = window->findChild<QObject*>(QStringLiteral("deleteTagDialog"));
        QVERIFY(deleteDialog);
        QTRY_VERIFY(deleteDialog->property("opened").toBool());
        QMetaObject::invokeMethod(deleteDialog, "accept");
        QTRY_COMPARE(context.tags().rowCount(), 9);
        QVERIFY(!context.project().tags().find(QStringLiteral("Temperature")));
        context.undoStack().undo();
        QCOMPARE(context.tags().rowCount(), 10);

        // Connection setup and tag creation work through their QML dialogs.
        auto* connectionsButton = window->findChild<QObject*>(QStringLiteral("connectionsButton"));
        auto* connectionsDialog = window->findChild<QObject*>(QStringLiteral("connectionsDialog"));
        QVERIFY(connectionsButton && connectionsDialog);
        QMetaObject::invokeMethod(connectionsButton, "click");
        QTRY_VERIFY(connectionsDialog->property("opened").toBool());
        auto* connections = context.tags().connections();
        connections->setDraftField(QStringLiteral("name"), QStringLiteral("LinePLC"));
        connections->setDraftField(QStringLiteral("host"), QStringLiteral("fixture.invalid"));
        if (const QString path = qEnvironmentVariable("HMI_CONNECTIONS_SCREENSHOT");
            !path.isEmpty())
        {
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(path));
        }
        auto* saveConnection = window->findChild<QObject*>(QStringLiteral("saveConnectionButton"));
        QVERIFY(saveConnection);
        QMetaObject::invokeMethod(saveConnection, "click");
        QTRY_VERIFY(!connectionsDialog->property("opened").toBool());
        QCOMPARE(context.project().connections().size(), std::size_t(1));

        auto* newTagDialog = window->findChild<QObject*>(QStringLiteral("newTagDialog"));
        auto* newTagName = window->findChild<QObject*>(QStringLiteral("newTagName"));
        auto* createTag = window->findChild<QObject*>(QStringLiteral("createTagButton"));
        QVERIFY(newTagDialog && newTagName && createTag);
        QMetaObject::invokeMethod(newTagDialog, "open");
        QTRY_VERIFY(newTagDialog->property("opened").toBool());
        newTagName->setProperty("text", QStringLiteral("OperatorSetpoint"));
        if (const QString path = qEnvironmentVariable("HMI_NEW_TAG_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(path));
        }
        QMetaObject::invokeMethod(createTag, "click");
        QTRY_VERIFY(!newTagDialog->property("opened").toBool());
        QCOMPARE(context.tags().rowCount(), 11);
        QVERIFY(context.project().tags().find(QStringLiteral("OperatorSetpoint")));
        context.undoStack().undo();
        context.undoStack().undo();
        QCOMPARE(context.tags().rowCount(), 10);
        QVERIFY(context.project().connections().empty());

        if (const QString path = qEnvironmentVariable("HMI_TAGS_SCREENSHOT"); !path.isEmpty())
        {
            context.theme().setDensity(Theme::Density::Compact);
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(path));
        }
        auto* work = window->findChild<QQuickItem*>(QStringLiteral("workArea"));
        QVERIFY(work);
        work->setProperty("showingTags", false);
        QTRY_VERIFY(canvas->isVisible());

        // The Overview's screen window shows the P-101 details screen: a
        // preview in the editor, live in the simulator.
        if (const QString path = qEnvironmentVariable("HMI_OVERVIEW_SCREENSHOT"); !path.isEmpty())
        {
            QMetaObject::invokeMethod(menu, "close");
            context.editor().setCurrentScreen(1);
            context.theme().setDensity(Theme::Density::Compact);
            QTest::qWait(500);
            QVERIFY(window->grabWindow().save(path));
            context.simulation().start();
            QQuickWindow* live = nullptr;
            QTRY_VERIFY((live = simulatorWindow(engine)) != nullptr);
            QVERIFY(QTest::qWaitForWindowExposed(live));
            QTest::qWait(1200);
            QVERIFY(live->grabWindow().save(
                QString(path).replace(QStringLiteral(".png"), QStringLiteral("_live.png"))));
            context.simulation().stop();
            QTRY_VERIFY(simulatorWindow(engine) == nullptr);
            context.editor().setCurrentScreen(0);
        }

        // The runtime simulation opens a window of its own showing a play
        // copy of the screen at its size; the editor keeps the document.
        QMetaObject::invokeMethod(menu, "close");
        context.simulation().start();
        QQuickWindow* simulator = nullptr;
        QTRY_VERIFY((simulator = simulatorWindow(engine)) != nullptr);
        QVERIFY(QTest::qWaitForWindowExposed(simulator));
        auto* view = simulator->findChild<RuntimeView*>(QStringLiteral("runtimeView"));
        QVERIFY(view);
        const Screen* play = view->screenItem()->screen();
        QVERIFY(play && play != &screen);
        QCOMPARE(view->screenItem()->objectItemCount(), int(screen.objectCount()));
        QCOMPARE(canvas->screenItem()->screen(), &screen);
        QVERIFY(simulator->title().startsWith(screen.name()));
        const int undoBeforeOperation = context.undoStack().index();
        const auto operate = [&](const QString& name)
        {
            const auto target = std::ranges::find_if(play->objects(), [&](const auto& item)
                                                     { return item->name() == name; });
            if (target == play->objects().end())
                return false;
            QTest::mouseClick(
                simulator, Qt::LeftButton, {},
                view->screenItem()->mapToScene((*target)->sceneBoundingRect().center()).toPoint());
            return true;
        };
        QVERIFY(operate(QStringLiteral("Start transfer")));
        QTRY_COMPARE(context.tagEngine().value(QStringLiteral("RunCommand")), TagValue(true));
        QTRY_COMPARE(context.tagEngine().value(QStringLiteral("PumpRunning")), TagValue(true));
        QVERIFY(operate(QStringLiteral("Flow setpoint")));
        QTRY_VERIFY(context.simulation().input()->isOpen());
        auto* valueEntry = simulator->findChild<QQuickItem*>(QStringLiteral("runtimeValueEntry"));
        QVERIFY(valueEntry);
        QTRY_VERIFY(valueEntry->hasActiveFocus());
        QTest::keyClick(simulator, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(simulator, Qt::Key_3);
        QTest::keyClick(simulator, Qt::Key_2);
        QCOMPARE(context.simulation().input()->text(), QStringLiteral("32"));
        if (const QString path = qEnvironmentVariable("HMI_RUNTIME_INPUT_SCREENSHOT");
            !path.isEmpty())
        {
            QTest::qWait(150);
            QVERIFY(simulator->grabWindow().save(path));
        }
        auto* applyValue = simulator->findChild<QObject*>(QStringLiteral("runtimeApply"));
        QVERIFY(applyValue);
        QMetaObject::invokeMethod(applyValue, "click");
        QTRY_VERIFY(!context.simulation().input()->isOpen());
        QCOMPARE(context.tagEngine().value(QStringLiteral("FlowSetpoint")), TagValue(32.0));
        for (const auto& item : play->objects())
        {
            const auto* binding = item->binding(Prop::Value);
            if (binding && binding->tag == QStringLiteral("FlowSetpoint"))
                QTRY_COMPARE(item->value<double>(Prop::Value), 32.0);
        }
        QCOMPARE(context.undoStack().index(), undoBeforeOperation);
        if (const QString path = qEnvironmentVariable("HMI_SIMULATE_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(1500);
            QVERIFY(simulator->grabWindow().save(path));
        }

        // A popup opens over the screen, in a frame with a close button.
        QVERIFY(context.simulation().openPopup(QStringLiteral("P-101 details")));
        QQuickItem* popupView = nullptr;
        QTRY_VERIFY((popupView = simulator->findChild<RuntimeView*>(QStringLiteral("popupView"))) !=
                    nullptr);
        QTRY_VERIFY(static_cast<RuntimeView*>(popupView)->screenItem()->objectItemCount() > 0);
        if (const QString path = qEnvironmentVariable("HMI_POPUP_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(1000);
            QVERIFY(simulator->grabWindow().save(path));
        }
        auto* closePopup = simulator->findChild<QQuickItem*>(QStringLiteral("closePopupButton"));
        QVERIFY(closePopup);
        QMetaObject::invokeMethod(closePopup, "click");
        QTRY_VERIFY(!context.simulation().popupOpen());
        QTRY_VERIFY(simulator->findChild<RuntimeView*>(QStringLiteral("popupView")) == nullptr);
        QVERIFY(operate(QStringLiteral("Stop transfer")));
        QTRY_COMPARE(context.tagEngine().value(QStringLiteral("RunCommand")), TagValue(false));
        QTRY_COMPARE(context.tagEngine().value(QStringLiteral("PumpRunning")), TagValue(false));
        // Closing the window stops the simulation.
        simulator->close();
        QTRY_VERIFY(!context.simulation().isRunning());
        QTRY_VERIFY(simulatorWindow(engine) == nullptr);

        if (const QString path = qEnvironmentVariable("HMI_SCREENSHOT"); !path.isEmpty())
        {
            QMetaObject::invokeMethod(menu, "close");
            context.theme().setDensity(Theme::Density::Compact);
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(path));
        }
    }
};

QTEST_MAIN(EditorShellTest)
#include "EditorShellTest.moc"
