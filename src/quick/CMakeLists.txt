# hmi_quick: C++ Qt Quick items for the canvas (viewport, screen, object
# items, overlays) and the gesture recognizer. Shared by the editor and the
# later runtime; knows nothing about editor tools or commands.
find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_add_library(hmi_quick STATIC)

qt_add_qml_module(hmi_quick
    URI HmiStudio.Quick
    VERSION 1.0
    OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml/HmiStudio/Quick"
    SOURCES
        CanvasItem.cpp
        CanvasItem.h
        ICanvasView.h
        ObjectItem.cpp
        ObjectItem.h
        ScreenItem.cpp
        ScreenItem.h
        input/GestureRecognizer.cpp
        input/GestureRecognizer.h
        input/InputSettings.h
        input/PointerGesture.h
        overlays/GridItem.cpp
        overlays/GridItem.h
        overlays/RulerItem.cpp
        overlays/RulerItem.h
        overlays/SceneGraphShapes.cpp
        overlays/SceneGraphShapes.h
        overlays/SelectionGeometry.cpp
        overlays/SelectionGeometry.h
        overlays/SelectionOverlay.cpp
        overlays/SelectionOverlay.h
)

target_include_directories(hmi_quick PUBLIC "${PROJECT_SOURCE_DIR}/src")
# The generated QML type registration includes headers by file name only.
target_include_directories(hmi_quick PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/overlays")
target_link_libraries(hmi_quick PUBLIC hmi::core Qt6::Quick)
hmi_set_project_options(hmi_quick)
