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
        app/EditorController.cpp
        app/EditorController.h
        app/SampleProject.cpp
        app/SampleProject.h
        app/Theme.cpp
        app/Theme.h
        commands/AddObjectCommand.cpp
        commands/AddObjectCommand.h
        commands/MoveObjectsCommand.cpp
        commands/MoveObjectsCommand.h
        commands/ResizeObjectCommand.cpp
        commands/ResizeObjectCommand.h
        models/SelectionModel.cpp
        models/SelectionModel.h
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
        tools/SelectTool.cpp
        tools/SelectTool.h
        tools/ToolContext.cpp
        tools/ToolContext.h
        tools/ToolController.cpp
        tools/ToolController.h
    QML_FILES
        qml/Main.qml
        qml/controls/IconButton.qml
        qml/panels/BottomPanel.qml
        qml/panels/CanvasPanel.qml
        qml/panels/PropertiesPanel.qml
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
)

target_include_directories(hmi_editor_lib PUBLIC "${PROJECT_SOURCE_DIR}/src")
# The generated QML type registration includes headers by file name only.
target_include_directories(hmi_editor_lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/models")
target_link_libraries(hmi_editor_lib PUBLIC
    hmi_quick
    hmi_quickplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg)
hmi_set_project_options(hmi_editor_lib)

qt_add_executable(hmi_editor WIN32 app/main.cpp)
target_link_libraries(hmi_editor PRIVATE hmi_editor_lib hmi_editor_libplugin)
hmi_set_project_options(hmi_editor)
set_target_properties(hmi_editor PROPERTIES OUTPUT_NAME "HmiStudio")
