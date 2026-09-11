cmake_minimum_required(VERSION 3.22)
project(Synera VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

function(synera_target target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/src")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(synera_core STATIC
    src/core/board.cpp src/core/bench.cpp src/core/player.cpp
    src/core/gamestate.cpp src/core/gamecontroller.cpp src/core/levelconfig.cpp
    src/core/randomstream.cpp
    src/entity/unit.cpp src/entity/heroes.cpp src/entity/herodata.cpp src/entity/equipment.cpp
    src/systems/combatsystem.cpp src/systems/combatcontext.cpp src/systems/pathfinder.cpp
    src/systems/shopsystem.cpp src/systems/traitsystem.cpp
    src/systems/equipmentsystem.cpp src/systems/savemanager.cpp)
synera_target(synera_core)
target_link_libraries(synera_core PUBLIC Qt6::Core)

add_library(synera_widgets STATIC
    src/gui/griditem.cpp src/gui/unititem.cpp src/gui/boardview.cpp
    src/gui/gamewindow.cpp src/gui/equipmentbutton.cpp)
synera_target(synera_widgets)
target_link_libraries(synera_widgets PUBLIC synera_core Qt6::Widgets)

# Shared runner retains the original --selftest entry point without a GUI dependency.
add_library(synera_checks STATIC src/core/selftest.cpp)
synera_target(synera_checks)
target_link_libraries(synera_checks PUBLIC synera_core)
add_executable(synera WIN32 src/main.cpp)
synera_target(synera)
target_link_libraries(synera PRIVATE synera_widgets synera_checks)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_executable(synera_widget_tests tests/widgets.cpp)
    synera_target(synera_widget_tests)
    target_link_libraries(synera_widget_tests PRIVATE synera_widgets Qt6::Test)
    add_test(NAME widgets COMMAND synera_widget_tests -platform offscreen -o "${CMAKE_CURRENT_BINARY_DIR}/widget_report.txt,txt")
    set_tests_properties(widgets PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(widgets PROPERTIES ENVIRONMENT "QT_QPA_FONTDIR=$ENV{SystemRoot}/Fonts")
    endif()
    add_executable(synera_selftest tests/main.cpp)
    synera_target(synera_selftest)
    target_link_libraries(synera_selftest PRIVATE synera_checks)
    add_test(NAME core COMMAND synera_selftest --report "${CMAKE_CURRENT_BINARY_DIR}/selftest_report.txt")
    set_tests_properties(core PROPERTIES TIMEOUT 120)
    if(WIN32)
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set_tests_properties(core widgets PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${compiler_bin}")
    endif()
endif()

include(cmake/WindowsRuntime.cmake)
