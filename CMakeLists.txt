cmake_minimum_required(VERSION 3.25)

project(HmiStudio
    VERSION 0.1.0
    DESCRIPTION "Desktop and touch editor for HMI screens"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(HMI_BUILD_TESTS "Build the unit tests" ON)
option(HMI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

# Sanitizers default on for Linux debug builds only; MinGW and MSVC support is
# incomplete.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_hmiSanitizersDefault ON)
else()
    set(_hmiSanitizersDefault OFF)
endif()
option(HMI_ENABLE_SANITIZERS "Build with AddressSanitizer and UBSan" ${_hmiSanitizersDefault})

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)
qt_standard_project_setup(REQUIRES 6.8)

include(CompilerWarnings)
include(Dependencies)

add_subdirectory(src)

if(HMI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
