# hmi_comm: tag values at runtime. IDriver and its implementations (the
# simulator and optional native PLC adapters), the TagEngine that runs them on a
# worker thread, and the BindingEvaluator that drives a screen from tag
# values. Depends on hmi_core only (ARCHITECTURE.md section 8).
add_library(hmi_comm STATIC
    BindingEvaluator.cpp
    BindingEvaluator.h
    IDriver.h
    PlcDriver.cpp
    PlcDriver.h
    SimulatorDriver.cpp
    SimulatorDriver.h
    TagEngine.cpp
    TagEngine.h
)

target_include_directories(hmi_comm PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(hmi_comm PUBLIC hmi::core Qt6::Core)
set_target_properties(hmi_comm PROPERTIES AUTOMOC ON)
hmi_set_project_options(hmi_comm)
