cmake_minimum_required(VERSION 3.16)
project(verity_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VERITY_ENGINE_BUILD_TESTS "Build engine tests" ON)
option(VERITY_ENGINE_BUILD_BENCH "Build engine microbenchmarks" OFF)

add_library(verity_engine STATIC
    src/engine.cpp
    include/verity/engine.hpp
)
target_include_directories(verity_engine PUBLIC include)

if(VERITY_ENGINE_BUILD_TESTS)
  enable_testing()
  add_executable(engine_tests tests/engine_tests.cpp)
  target_link_libraries(engine_tests PRIVATE verity_engine)
  add_test(NAME engine_smoke COMMAND engine_tests)
endif()

if(VERITY_ENGINE_BUILD_BENCH)
  add_executable(engine_bench bench/curve_bench.cpp)
  target_link_libraries(engine_bench PRIVATE verity_engine)
endif()
