# hmi_editor_lib: controllers, tools, commands and panel models plus the QML
# shell, as a static QML module so tests can link the C++ without the app.
# hmi_editor: the executable (main.cpp only).
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Svg)

qt_add_library(hmi_editor_lib STATIC)

qt_add_qml_module(hmi_editor_lib
    URI HmiStudio.Editor
    VERSION 1.0
    OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml/HmiStudio/Editor"
    IMPORTS HmiStudio.Quick
    SOURCES
        app/AppContext.cpp
        app/AppContext.h
        app/AppSettings.cpp
        app/AppSettings.h
        app/DocumentController.cpp
        app/DocumentController.h
        app/EditActions.cpp
        app/EditActions.h
        app/EditorController.cpp
        app/EditorController.h
        app/SampleProject.cpp
        app/SampleProject.h
        app/FillingStationDriver.cpp
        app/FillingStationDriver.h
        app/SimulationController.cpp
        app/SimulationController.h
        app/Theme.cpp
        app/Theme.h
        commands/AddObjectCommand.cpp
        commands/AddObjectCommand.h
        commands/ChangeStyleCommand.cpp
        commands/ChangeStyleCommand.h
        commands/GroupObjectsCommand.cpp
        commands/GroupObjectsCommand.h
        commands/MoveObjectsCommand.cpp
        commands/MoveObjectsCommand.h
        commands/RemoveObjectsCommand.cpp
        commands/RemoveObjectsCommand.h
        commands/ReorderCommand.cpp
        commands/ReorderCommand.h
        commands/ResizeObjectCommand.cpp
        commands/ResizeObjectCommand.h
        commands/ScreenCommands.cpp
        commands/ScreenCommands.h
        commands/SetPropertyCommand.cpp
        commands/SetPropertyCommand.h
        commands/EventCommands.cpp
        commands/EventCommands.h
        commands/TagCommands.cpp
        commands/TagCommands.h
        commands/UngroupCommand.cpp
        commands/UngroupCommand.h
        models/EventModel.cpp
        models/EventModel.h
        models/LayerModel.cpp
        models/LayerModel.h
        models/PropertyModel.cpp
        models/PropertyModel.h
        models/ScreenListModel.cpp
        models/ScreenListModel.h
        models/SelectionModel.cpp
        models/SelectionModel.h
        models/TagConnections.cpp
        models/TagConnections.h
        models/TagModel.cpp
        models/TagModel.h
        models/ToolboxDragController.cpp
        models/ToolboxDragController.h
        models/ToolboxModel.cpp
        models/ToolboxModel.h
        tools/CreateTool.cpp
        tools/CreateTool.h
        tools/ITool.h
        tools/ObjectPlacement.cpp
        tools/ObjectPlacement.h
        tools/PanTool.cpp
        tools/PanTool.h
        tools/PathTool.cpp
        tools/PathTool.h
        tools/SelectTool.cpp
        tools/SelectTool.h
        tools/ToolContext.cpp
        tools/ToolContext.h
        tools/ToolController.cpp
        tools/ToolController.h
    QML_FILES
        qml/DocumentDialogs.qml
        qml/Main.qml
        qml/SimulatorWindow.qml
        qml/RuntimeValueDialog.qml
        qml/controls/IconButton.qml
        qml/panels/BottomPanel.qml
        qml/panels/CanvasContextMenu.qml
        qml/panels/CanvasPanel.qml
        qml/panels/EventsPanel.qml
        qml/panels/LayerRow.qml
        qml/panels/PropertiesPanel.qml
        qml/panels/properties/AssetEditor.qml
        qml/panels/properties/BindingPopup.qml
        qml/panels/properties/BoolEditor.qml
        qml/panels/properties/ColorEditor.qml
        qml/panels/properties/EnumEditor.qml
        qml/panels/properties/FontEditor.qml
        qml/panels/properties/NumberEditor.qml
        qml/panels/properties/NumberKeypad.qml
        qml/panels/properties/PairEditor.qml
        qml/panels/properties/PropertyLabel.qml
        qml/panels/properties/ScreenPickerEditor.qml
        qml/panels/properties/TagPickerEditor.qml
        qml/panels/properties/TextEditor.qml
        qml/panels/SelectionToolbar.qml
        qml/panels/TagsPanel.qml
        qml/panels/NewTagDialog.qml
        qml/panels/ConnectionsDialog.qml
        qml/panels/StatusBar.qml
        qml/panels/ToolboxPanel.qml
        qml/panels/ToolboxTile.qml
        qml/panels/TopBar.qml
)

qt_add_resources(hmi_editor_lib "hmi_icons"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/icons/ellipse.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/line.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/rectangle.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/text-box.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/tool-pan.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/tool-select.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/undo.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/redo.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/zoom-fit.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/diagnostics-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/recipe-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/media-player.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/browser.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/screen-window.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/fx-trend-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/trend-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/alarm-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/sensor.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/conveyor.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/exchanger.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/pipe.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/motor.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/valve.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/pump.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/tank.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/touch-area.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/clock.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/gauge.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/slider.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/bar.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/symbolic-io-field.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/list-box.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/radio-group.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/check-box.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/switch.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/button.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/io-field.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/graphic-view.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/circle.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/polygon.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/polyline.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/drag.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/eye.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/eye-off.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/keypad.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/lock.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/minus.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/panel-bottom.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/panel-left.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/panel-right.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/link.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/play.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/stop.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/plus.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/unlock.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/arrow-down.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/arrow-left.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/arrow-right.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/arrow-up.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/bring-forward.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/delete.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/duplicate.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/group.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/more.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/send-backward.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/snap.svg"
        "${PROJECT_SOURCE_DIR}/resources/icons/ungroup.svg"
)

target_include_directories(hmi_editor_lib PUBLIC "${PROJECT_SOURCE_DIR}/src")
# The generated QML type registration includes headers by file name only.
target_include_directories(hmi_editor_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/models")
target_link_libraries(hmi_editor_lib PUBLIC
    hmi_quick
    hmi_comm
    hmi_render
    hmi_quickplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg)
hmi_set_project_options(hmi_editor_lib)

qt_add_executable(hmi_editor WIN32 app/main.cpp)
target_link_libraries(hmi_editor PRIVATE hmi_editor_lib hmi_editor_libplugin)
hmi_set_project_options(hmi_editor)
set_target_properties(hmi_editor PROPERTIES OUTPUT_NAME "HmiStudio")
