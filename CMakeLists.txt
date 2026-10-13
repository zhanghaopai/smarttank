cmake_minimum_required(VERSION 3.29)
project(SmartTank VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "SmartTank currently requires Windows and MSVC.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Test)
find_package(OpenCASCADE CONFIG REQUIRED)

add_library(smarttank_core src/core/Project.cpp)
target_include_directories(smarttank_core PUBLIC src)
target_compile_options(smarttank_core PRIVATE /W4 /utf-8)

add_library(smarttank_ui
    src/ui/MainWindow.cpp
    src/view/EnvelopeGeometry.cpp
    src/view/OcctView.cpp)
target_include_directories(smarttank_ui PUBLIC src PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(smarttank_ui PUBLIC smarttank_core Qt6::Widgets
    TKernel TKMath TKG3d TKGeomBase TKBRep TKTopAlgo TKPrim TKV3d TKOpenGl TKService)
target_compile_options(smarttank_ui PRIVATE /W4 /utf-8)

qt_add_executable(smarttank src/main.cpp)
target_link_libraries(smarttank PRIVATE smarttank_ui)
target_compile_options(smarttank PRIVATE /W4 /utf-8)

include(CTest)
if(BUILD_TESTING)
    add_executable(project_tests tests/ProjectTests.cpp)
    target_link_libraries(project_tests PRIVATE smarttank_core)
    target_compile_options(project_tests PRIVATE /W4 /utf-8)
    add_test(NAME project-state COMMAND project_tests)
    add_executable(workspace_tests tests/WorkspaceTests.cpp)
    target_link_libraries(workspace_tests PRIVATE smarttank_ui Qt6::Test)
    target_compile_options(workspace_tests PRIVATE /W4 /utf-8)
    add_test(NAME workspace COMMAND workspace_tests -platform offscreen)
    add_test(NAME application-startup COMMAND smarttank --smoke-test -platform offscreen)
    set_tests_properties(project-state workspace application-startup PROPERTIES TIMEOUT 30)
endif()
