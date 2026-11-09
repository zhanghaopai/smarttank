cmake_minimum_required(VERSION 3.29)
project(SmartTank VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "SmartTank currently requires Windows and MSVC.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
find_package(OpenCASCADE CONFIG REQUIRED)

add_library(smarttank_core src/core/Project.cpp)
target_include_directories(smarttank_core PUBLIC src)
target_compile_options(smarttank_core PRIVATE /W4 /utf-8)

add_library(smarttank_geometry
    src/geometry/EnvelopeGeometry.cpp
    src/geometry/InstalledGeometry.cpp)
target_include_directories(smarttank_geometry PUBLIC src ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(smarttank_geometry PUBLIC smarttank_core
    TKernel TKMath TKG3d TKGeomBase TKBRep TKTopAlgo TKPrim)
target_compile_options(smarttank_geometry PRIVATE /W4 /utf-8)

add_library(smarttank_view
    src/view/OcctView.cpp
    src/view/OcctViewPresentation.cpp)
set_target_properties(smarttank_view PROPERTIES AUTOMOC ON)
target_include_directories(smarttank_view PUBLIC src)
target_link_libraries(smarttank_view PUBLIC smarttank_geometry Qt6::Widgets
    TKV3d TKOpenGl TKService)
target_compile_options(smarttank_view PRIVATE /W4 /utf-8)

add_library(smarttank_ui
    src/ui/MainWindow.cpp
    src/ui/MainWindowLayout.cpp
    src/ui/MainWindowActions.cpp
    src/ui/ParameterEditor.cpp)
set_target_properties(smarttank_ui PROPERTIES AUTOMOC ON)
target_include_directories(smarttank_ui PUBLIC src)
target_link_libraries(smarttank_ui PUBLIC smarttank_view)
target_compile_options(smarttank_ui PRIVATE /W4 /utf-8)

qt_add_executable(smarttank src/main.cpp)
target_link_libraries(smarttank PRIVATE smarttank_ui)
target_compile_options(smarttank PRIVATE /W4 /utf-8)

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_executable(project_tests tests/ProjectTests.cpp)
    target_link_libraries(project_tests PRIVATE smarttank_core)
    target_compile_options(project_tests PRIVATE /W4 /utf-8)
    add_test(NAME project-state COMMAND project_tests)
    add_executable(workspace_tests tests/WorkspaceTests.cpp)
    set_target_properties(workspace_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(workspace_tests PRIVATE smarttank_ui Qt6::Test)
    target_compile_options(workspace_tests PRIVATE /W4 /utf-8)
    add_test(NAME workspace COMMAND workspace_tests -platform offscreen)
    add_test(NAME application-startup COMMAND smarttank --smoke-test -platform offscreen)
    set_tests_properties(project-state workspace application-startup PROPERTIES TIMEOUT 30)
endif()
