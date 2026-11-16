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
hmi_add_test(GroupObjectTest SOURCES core/model/GroupObjectTest.cpp core/objects/ObjectContract.h)
hmi_add_test(ObjectJsonTest SOURCES core/io/ObjectJsonTest.cpp)
hmi_add_test(HmiStyleTest SOURCES core/model/HmiStyleTest.cpp)
hmi_add_test(TagsTest SOURCES core/tags/TagsTest.cpp)
hmi_add_test(BindingTest SOURCES core/tags/BindingTest.cpp)
hmi_add_test(ProjectSerializerTest SOURCES core/io/ProjectSerializerTest.cpp)
hmi_add_test(PropertyDescriptorTest SOURCES core/property/PropertyDescriptorTest.cpp)
hmi_add_test(ObjectRegistryTest SOURCES core/registry/ObjectRegistryTest.cpp)
hmi_add_test(HitTesterTest SOURCES core/spatial/HitTesterTest.cpp)
hmi_add_test(SnapperTest SOURCES core/spatial/SnapperTest.cpp)
hmi_add_test(RectangleTest SOURCES core/objects/RectangleTest.cpp core/objects/ObjectContract.h)
hmi_add_test(EllipseTest SOURCES core/objects/EllipseTest.cpp core/objects/ObjectContract.h)
hmi_add_test(LineTest SOURCES core/objects/LineTest.cpp core/objects/ObjectContract.h)
hmi_add_test(TextBoxTest SOURCES core/objects/TextBoxTest.cpp core/objects/ObjectContract.h)
hmi_add_test(AllObjectsTest SOURCES core/objects/AllObjectsTest.cpp core/objects/ObjectContract.h)

# Graphics
hmi_add_test(GraphicsTest SOURCES render/GraphicsTest.cpp LIBS hmi_render)

# Tags at runtime
hmi_add_test(SimulationTest SOURCES comm/SimulationTest.cpp LIBS hmi_comm)

# Qt Quick items and input
hmi_add_test(SelectionGeometryTest SOURCES quick/SelectionGeometryTest.cpp LIBS hmi_quick)
hmi_add_test(GestureRecognizerTest SOURCES quick/GestureRecognizerTest.cpp LIBS hmi_quick)
hmi_add_test(CanvasItemTest SOURCES quick/CanvasItemTest.cpp LIBS hmi_quick)
hmi_add_test(LargeScreenTest SOURCES quick/LargeScreenTest.cpp LIBS hmi_quick)

# Editor
set(_hmiEditorTestSupport editor/FakeCanvas.h editor/GestureFactory.h)
hmi_add_test(CommandsTest SOURCES editor/CommandsTest.cpp LIBS hmi_editor_lib)
hmi_add_test(EditActionsTest SOURCES editor/EditActionsTest.cpp LIBS hmi_editor_lib)
hmi_add_test(SelectToolTest SOURCES editor/SelectToolTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(CreateToolTest SOURCES editor/CreateToolTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(PathToolTest SOURCES editor/PathToolTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(ToolControllerTest SOURCES editor/ToolControllerTest.cpp ${_hmiEditorTestSupport}
             LIBS hmi_editor_lib)
hmi_add_test(ToolboxTest SOURCES editor/ToolboxTest.cpp LIBS hmi_editor_lib)
hmi_add_test(PropertyModelTest SOURCES editor/PropertyModelTest.cpp LIBS hmi_editor_lib)
hmi_add_test(TagModelTest SOURCES editor/TagModelTest.cpp LIBS hmi_editor_lib)
hmi_add_test(LayerModelTest SOURCES editor/LayerModelTest.cpp LIBS hmi_editor_lib)
hmi_add_test(DocumentControllerTest SOURCES editor/DocumentControllerTest.cpp LIBS hmi_editor_lib)
hmi_add_test(EditorShellTest SOURCES editor/EditorShellTest.cpp
             LIBS hmi_editor_lib hmi_editor_libplugin)
