# Top-level build: core algorithms are shared by the CLI, GUI, and tests.
# Optional dependencies can be disabled to build just the command-line app.

cmake_minimum_required(VERSION 3.20)
project(MapaGator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared code stays in one library so the CLI, GUI, and tests use the same algorithms.
add_library(mapagator_core
    src/Graph.cpp
    src/MinHeap.cpp
    src/UnionFind.cpp
    src/Prim.cpp
    src/Kruskal.cpp
    src/CostScenario.cpp
    src/Analysis.cpp
    src/MapRender.cpp
)
target_include_directories(mapagator_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(mapagator_core PRIVATE /W4)
else()
    target_compile_options(mapagator_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(MapaGator apps/main.cpp)
target_link_libraries(MapaGator PRIVATE mapagator_core)

option(MAPAGATOR_BUILD_TESTS "Build Catch2 v3 tests" ON)
option(MAPAGATOR_FETCH_CATCH2 "Download Catch2 when missing" ON)
option(MAPAGATOR_BUILD_SFML "Build SFML 2.x viewer" OFF)

if(MAPAGATOR_BUILD_TESTS)
    include(CTest)
    find_package(Catch2 3 QUIET)

    if(NOT Catch2_FOUND)
        if(NOT MAPAGATOR_FETCH_CATCH2)
            message(FATAL_ERROR "Install Catch2 v3 or enable MAPAGATOR_FETCH_CATCH2")
        endif()
        include(FetchContent)
        FetchContent_Declare(Catch2
            URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.1.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(MapaGatorTests tests/test_mapagator.cpp)
    target_link_libraries(MapaGatorTests PRIVATE mapagator_core Catch2::Catch2WithMain)

    if(DEFINED catch2_SOURCE_DIR)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
    include(Catch)
    catch_discover_tests(MapaGatorTests)
endif()

if(MAPAGATOR_BUILD_SFML)
    # The interactive viewer uses SFML 2.x, not the SFML 3 API.
    find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
    add_executable(MapaGatorGUI apps/sfml_viewer.cpp)
    target_link_libraries(MapaGatorGUI PRIVATE
        mapagator_core sfml-graphics sfml-window sfml-system)
endif()
