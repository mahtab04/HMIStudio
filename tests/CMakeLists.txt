find_package(Qt6 REQUIRED COMPONENTS Test)

# On Windows the tests need the Qt and compiler runtime DLLs on PATH.
get_target_property(_hmiQmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_hmiQtBinDir "${_hmiQmake}" DIRECTORY)
get_filename_component(_hmiCompilerDir "${CMAKE_CXX_COMPILER}" DIRECTORY)

# hmi_add_test(<name> SOURCES <files>... [LIBS <targets>...])
#
# One Qt Test executable per test class, run headless with the offscreen
# platform plugin. Links hmi::core and Qt Test plus LIBS.
function(hmi_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBS")
    add_executable(${name} ${arg_SOURCES})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE hmi::core Qt6::Test ${arg_LIBS})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    hmi_set_project_options(${name})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_hmiQtBinDir};PATH=path_list_prepend:${_hmiCompilerDir}")
endfunction()

# Core
hmi_add_test(IdsTest SOURCES core/model/IdsTest.cpp)
hmi_add_test(ScreenTest SOURCES core/model/ScreenTest.cpp)
hmi_add_test(ProjectTest SOURCES core/model/ProjectTest.cpp)
hmi_add_test(PropertyDescriptorTest SOURCES core/property/PropertyDescriptorTest.cpp)
hmi_add_test(ObjectRegistryTest SOURCES core/registry/ObjectRegistryTest.cpp)
hmi_add_test(HitTesterTest SOURCES core/spatial/HitTesterTest.cpp)
hmi_add_test(RectangleTest SOURCES core/objects/RectangleTest.cpp core/objects/ObjectContract.h)
hmi_add_test(EllipseTest SOURCES core/objects/EllipseTest.cpp core/objects/ObjectContract.h)
hmi_add_test(LineTest SOURCES core/objects/LineTest.cpp core/objects/ObjectContract.h)
hmi_add_test(TextBoxTest SOURCES core/objects/TextBoxTest.cpp core/objects/ObjectContract.h)

# Qt Quick items and input
hmi_add_test(SelectionGeometryTest SOURCES quick/SelectionGeometryTest.cpp LIBS hmi_quick)
hmi_add_test(GestureRecognizerTest SOURCES quick/GestureRecognizerTest.cpp LIBS hmi_quick)
hmi_add_test(CanvasItemTest SOURCES quick/CanvasItemTest.cpp LIBS hmi_quick)

# Editor
set(_hmiEditorTestSupport editor/FakeCanvas.h editor/GestureFactory.h)
hmi_add_test(CommandsTest SOURCES editor/CommandsTest.cpp LIBS hmi_editor_lib)
hmi_add_test(SelectToolTest SOURCES editor/SelectToolTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(CreateToolTest SOURCES editor/CreateToolTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(ToolControllerTest SOURCES editor/ToolControllerTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(ToolboxTest SOURCES editor/ToolboxTest.cpp LIBS hmi_editor_lib)
hmi_add_test(EditorShellTest SOURCES editor/EditorShellTest.cpp
             LIBS hmi_editor_lib hmi_editor_libplugin)
