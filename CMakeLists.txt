cmake_minimum_required(VERSION 3.16)
project(sim_playground LANGUAGES CXX)

# Common
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# src for headers and core (if any)
add_library(project_headers INTERFACE)
target_include_directories(project_headers
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 2) src for static library with .cpp (if any)
file(GLOB SRC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(SRC_SOURCES)
    add_library(project_core STATIC ${SRC_SOURCES})
    target_include_directories(project_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

# enter simulator subdirectory
add_subdirectory(simulator)
