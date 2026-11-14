find_package(Qt6 REQUIRED COMPONENTS Test)

# On Windows the tests need the Qt and compiler runtime DLLs on PATH.
get_target_property(_hmiQmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_hmiQtBinDir "${_hmiQmake}" DIRECTORY)
get_filename_component(_hmiCompilerDir "${CMAKE_CXX_COMPILER}" DIRECTORY)

# hmi_add_test(<name> <sources>...)
#
# One Qt Test executable per test class, run headless with the offscreen
# platform plugin.
function(hmi_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${name} PRIVATE hmi::core Qt6::Test)
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    hmi_set_project_options(${name})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_hmiQtBinDir};PATH=path_list_prepend:${_hmiCompilerDir}")
endfunction()

hmi_add_test(IdsTest core/model/IdsTest.cpp)
hmi_add_test(ScreenTest core/model/ScreenTest.cpp)
hmi_add_test(ProjectTest core/model/ProjectTest.cpp)
hmi_add_test(PropertyDescriptorTest core/property/PropertyDescriptorTest.cpp)
hmi_add_test(ObjectRegistryTest core/registry/ObjectRegistryTest.cpp)
hmi_add_test(HitTesterTest core/spatial/HitTesterTest.cpp)
hmi_add_test(RectangleTest core/objects/RectangleTest.cpp core/objects/ObjectContract.h)
hmi_add_test(EllipseTest core/objects/EllipseTest.cpp core/objects/ObjectContract.h)
hmi_add_test(LineTest core/objects/LineTest.cpp core/objects/ObjectContract.h)
hmi_add_test(TextBoxTest core/objects/TextBoxTest.cpp core/objects/ObjectContract.h)
