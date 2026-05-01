cmake_minimum_required(VERSION 3.21)

find_package(Catch2 CONFIG REQUIRED)

add_executable(SkyscribeTests
    test_output_parser.cpp
)

target_link_libraries(SkyscribeTests PRIVATE
    SkyscribeLib
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(SkyscribeTests)
