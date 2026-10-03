cmake_minimum_required(VERSION 3.16)
project(flip7 LANGUAGES CXX)

option(FLIP7_BUILD_TESTS "Build Flip 7 tests" ON)

find_package(Threads REQUIRED)

add_library(flip7
    src/env/deck.cpp
    src/env/environment.cpp
    src/env/card.cpp
    src/simulation.cpp
    src/strategies/strategies.cpp
    src/orchestration/parallel_runner.cpp
    src/experiment/aggregate.cpp
    src/experiment/csv.cpp
)

target_include_directories(flip7 PUBLIC include)
target_compile_features(flip7 PUBLIC cxx_std_17)
target_link_libraries(flip7 PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(flip7 PRIVATE /W4 /permissive-)
else()
    target_compile_options(flip7 PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(flip7-sweep src/app/main.cpp)
target_link_libraries(flip7-sweep PRIVATE flip7)

if(FLIP7_BUILD_TESTS)
    enable_testing()
    add_executable(flip7-tests tests/test_flip7.cpp)
    target_link_libraries(flip7-tests PRIVATE flip7)
    add_test(NAME flip7-tests COMMAND flip7-tests)
endif()
