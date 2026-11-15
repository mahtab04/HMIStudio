// Loads the real QML shell offscreen. Set HMI_SCREENSHOT=<file.png> to save
// an image of the window for a visual check.

#include "core/model/Project.h"
#include "editor/app/AppContext.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/Theme.h"
#include "editor/models/SelectionModel.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"
#include "quick/CanvasItem.h"
#include "quick/ScreenItem.h"

#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(HmiStudio_QuickPlugin)
Q_IMPORT_QML_PLUGIN(HmiStudio_EditorPlugin)

using namespace hmi;

class EditorShellTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Fusion")); }

    void loadsAndAttachesCanvas()
    {
        // Any warning that mentions a QML file is a failure.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("\\.qml")));

        AppContext context(makeSampleProject());
        QGuiApplication::setPalette(context.theme().palette());
        context.theme().setDensity(Theme::Density::Compact);

        QQmlApplicationEngine engine;
        engine.setInitialProperties({
            {QStringLiteral("editor"), QVariant::fromValue(&context.editor())},
            {QStringLiteral("theme"), QVariant::fromValue(&context.theme())},
            {QStringLiteral("toolbox"), QVariant::fromValue(&context.toolbox())},
            {QStringLiteral("toolboxDrag"), QVariant::fromValue(&context.toolboxDrag())},
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

        // Right-click (or the toolbar's "more" button) opens the menu.
        auto* menu = window->findChild<QObject*>(QStringLiteral("contextMenu"));
        QVERIFY(menu);
        Q_EMIT context.editor().contextMenuRequested(QPointF(100, 100));
        QTRY_VERIFY(menu->property("opened").toBool());

        if (const QString path = qEnvironmentVariable("HMI_SCREENSHOT"); !path.isEmpty())
        {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(path));
        }
    }
};

QTEST_MAIN(EditorShellTest)
#include "EditorShellTest.moc"
