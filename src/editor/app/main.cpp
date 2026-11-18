#include "editor/app/AppContext.h"
#include "editor/app/AppSettings.h"
#include "editor/app/DocumentController.h"
#include "editor/app/EditorController.h"
#include "editor/app/SampleProject.h"
#include "editor/app/SimulationController.h"
#include "editor/app/Theme.h"
#include "editor/models/AlarmModel.h"
#include "editor/models/EventModel.h"
#include "editor/models/LayerModel.h"
#include "editor/models/PropertyModel.h"
#include "editor/models/ScreenListModel.h"
#include "editor/models/TagModel.h"
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
        {QStringLiteral("document"), QVariant::fromValue(&context.document())},
        {QStringLiteral("settings"), QVariant::fromValue(&context.settings())},
        {QStringLiteral("properties"), QVariant::fromValue(&context.properties())},
        {QStringLiteral("events"), QVariant::fromValue(&context.events())},
            {QStringLiteral("alarms"), QVariant::fromValue(&context.alarms())},
        {QStringLiteral("layers"), QVariant::fromValue(&context.layers())},
        {QStringLiteral("screens"), QVariant::fromValue(&context.screens())},
        {QStringLiteral("tags"), QVariant::fromValue(&context.tags())},
        {QStringLiteral("simulation"), QVariant::fromValue(&context.simulation())},
    });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("HmiStudio.Editor", "Main");

    return QGuiApplication::exec();
}
