cmake_minimum_required(VERSION 3.16)
project(verity_desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_SQLITE "Enable SQLite-backed storage for desktop" OFF)
option(ENABLE_QT_SHELL "Build Qt shell (requires Qt)" OFF)

add_library(verity_desktop STATIC
    src/command.cpp
    src/autosave.cpp
    src/replay.cpp
    include/verity/command.hpp
    include/verity/replay.hpp
)
target_include_directories(verity_desktop PUBLIC include)

if(ENABLE_SQLITE)
  find_package(SQLite3 REQUIRED)
  target_sources(verity_desktop PRIVATE src/db.cpp include/verity/db.hpp)
  target_link_libraries(verity_desktop PUBLIC SQLite::SQLite3)
  target_compile_definitions(verity_desktop PUBLIC VERITY_DESKTOP_SQLITE=1)
endif()

add_executable(verity_desktop_runner src/desktop_runner.cpp src/commands/add_keyframe.cpp src/commands/move_selection.cpp)
target_include_directories(verity_desktop_runner PRIVATE include)
target_link_libraries(verity_desktop_runner PRIVATE verity_desktop)

# Optional Qt shell scaffold (disabled by default)
if(ENABLE_QT_SHELL)
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6_FOUND)
    add_executable(verity_qt_shell src/main_qt.cpp)
    target_include_directories(verity_qt_shell PRIVATE include)
    target_link_libraries(verity_qt_shell PRIVATE verity_desktop Qt6::Widgets)
    target_compile_definitions(verity_qt_shell PRIVATE VERITY_QT_SHELL=1)
  else()
    message(WARNING "Qt6 not found; Qt shell will not be built.")
  endif()
endif()

# Tests (enabled when SQLite is on)
include(CTest)
if(ENABLE_SQLITE)
  enable_testing()
  add_executable(desktop_tests tests/commands_tests.cpp src/commands/add_keyframe.cpp src/commands/move_selection.cpp)
  target_include_directories(desktop_tests PRIVATE include)
  target_link_libraries(desktop_tests PRIVATE verity_desktop)
  add_test(NAME desktop_commands COMMAND desktop_tests)
endif()
