cmake_minimum_required(VERSION 3.21)
project(Skyscribe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── vcpkg packages ────────────────────────────────────────────────────────────
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ── Source files ──────────────────────────────────────────────────────────────
file(GLOB_RECURSE SKYSCRIBE_SOURCES
    "src/*.cpp"
    "src/*.h"
)

# ── Main target ───────────────────────────────────────────────────────────────
add_executable(Skyscribe WIN32 ${SKYSCRIBE_SOURCES})

target_include_directories(Skyscribe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(Skyscribe PRIVATE
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    dxgi
    d3dcompiler
    shell32
    ole32
)

target_compile_definitions(Skyscribe PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# ── Tests ─────────────────────────────────────────────────────────────────────
add_subdirectory(tests)
