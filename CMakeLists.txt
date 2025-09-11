cmake_minimum_required(VERSION 3.16)
project(sim_playground LANGUAGES CXX)

# Common settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === Interface target ===
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
)

add_library(project_headers INTERFACE)
target_include_directories(project_headers
    INTERFACE
        ${PROJECT_INCLUDE_DIRS}
)

# === Generate static library in scr/ ===
file(GLOB_RECURSE SRC_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(SRC_SOURCES)
    add_library(project_core STATIC ${SRC_SOURCES})
    target_include_directories(project_core PUBLIC ${PROJECT_INCLUDE_DIRS})
    target_compile_features(project_core PUBLIC cxx_std_17)
endif()

# aggregate output binary into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# simulators subdirectory
add_subdirectory(simulators)

# 4) tests: Debug only
if(NOT CMAKE_CONFIGURATION_TYPES) # single-config (Makefiles/Ninja)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_subdirectory(test)
    endif()
else()
    add_subdirectory(test)
endif()