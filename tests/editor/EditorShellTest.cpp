// Loads the real QML shell offscreen. Set HMI_SCREENSHOT=<file.png> to save
// an image of the window for a visual check.

#include "core/model/Project.h"
#include "editor/app/AppContext.h"
#include "editor/app/AppSettings.h"
#include "editor/app/DocumentController.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/SimulationController.h"
#include "editor/app/Theme.h"
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
        const HmiObject& object = *screen.objects()[2];
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

        // Right-click (or the toolbar's "more" button) opens the menu.
        auto* menu = window->findChild<QObject*>(QStringLiteral("contextMenu"));
        QVERIFY(menu);
        Q_EMIT context.editor().contextMenuRequested(QPointF(100, 100));
        QTRY_VERIFY(menu->property("opened").toBool());

        // The HMI tags tab opens the tag table in the work area.
        auto* tagsTab = window->findChild<QQuickItem*>(QStringLiteral("tagsTab"));
        QVERIFY(tagsTab);
        QMetaObject::invokeMethod(tagsTab, "click");
        QTRY_VERIFY(!canvas->isVisible());
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
        if (const QString path = qEnvironmentVariable("HMI_SIMULATE_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(1500);
            QVERIFY(simulator->grabWindow().save(path));
        }
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
