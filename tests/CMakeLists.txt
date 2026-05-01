cmake_minimum_required(VERSION 3.21)

find_package(Catch2 CONFIG REQUIRED)

# No test sources yet — harness only for Phase 0.
# Add test_*.cpp files here in later phases.

add_executable(SkyscribeTests
    # placeholder — Catch2 needs at least one TU to link
    catch_main.cpp
)

target_link_libraries(SkyscribeTests PRIVATE
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(SkyscribeTests)
