# hmi_comm: tag values at runtime. IDriver and its implementations (the
# simulator and optional native PLC adapters), the TagEngine that runs them on a
# worker thread, and the BindingEvaluator that drives a screen from tag
# values. Depends on hmi_core only (ARCHITECTURE.md section 8).
add_library(hmi_comm STATIC
    AlarmEngine.cpp
    AlarmEngine.h
    BindingEvaluator.cpp
    BindingEvaluator.h
    IDriver.h
    RuntimeAlarms.cpp
    RuntimeAlarms.h
    PlcDriver.cpp
    PlcDriver.h
    PlcProtocols.cpp
    PlcProtocols.h
    RuntimeDriver.cpp
    RuntimeDriver.h
    SimulatorDriver.cpp
    SimulatorDriver.h
    TagEngine.cpp
    TagEngine.h
    TestPlc.cpp
    TestPlc.h
)

find_package(Qt6 REQUIRED COMPONENTS Network)

target_include_directories(hmi_comm PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(hmi_comm PUBLIC hmi::core Qt6::Core Qt6::Network)
set_target_properties(hmi_comm PROPERTIES AUTOMOC ON)
hmi_set_project_options(hmi_comm)
