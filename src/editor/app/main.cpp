#include "editor/app/AppContext.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/Theme.h"
#include "editor/models/ToolboxDragController.h"
#include "editor/models/ToolboxModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdlib>

Q_IMPORT_QML_PLUGIN(HmiStudio_QuickPlugin)
Q_IMPORT_QML_PLUGIN(HmiStudio_EditorPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("HMI Studio"));
    QGuiApplication::setOrganizationName(QStringLiteral("HMI Studio"));

    // Fusion follows the application palette, so the Theme colours apply to
    // every control without per-control styling in QML.
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    hmi::AppContext context(hmi::makeSampleProject());
    QGuiApplication::setPalette(context.theme().palette());

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("editor"), QVariant::fromValue(&context.editor())},
        {QStringLiteral("theme"), QVariant::fromValue(&context.theme())},
        {QStringLiteral("toolbox"), QVariant::fromValue(&context.toolbox())},
        {QStringLiteral("toolboxDrag"), QVariant::fromValue(&context.toolboxDrag())},
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("HmiStudio.Editor", "Main");

    return QGuiApplication::exec();
}
