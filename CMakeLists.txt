cmake_minimum_required(VERSION 3.21)
project(Skyscribe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── vcpkg packages ────────────────────────────────────────────────────────────
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ── Shared logic library (compiler + project — used by main exe and tests) ────
file(GLOB_RECURSE SKYSCRIBE_LIB_SOURCES
    "src/compiler/*.cpp" "src/compiler/*.h"
    "src/project/*.cpp"  "src/project/*.h"
    "src/graph/*.h"
)
add_library(SkyscribeLib STATIC ${SKYSCRIBE_LIB_SOURCES})
target_include_directories(SkyscribeLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(SkyscribeLib PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(SkyscribeLib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_features(SkyscribeLib PUBLIC cxx_std_17)

# ── Main executable sources (app + ui, excluding lib sources) ─────────────────
file(GLOB_RECURSE SKYSCRIBE_APP_SOURCES
    "src/app/*.cpp" "src/app/*.h"
    "src/ui/*.cpp"  "src/ui/*.h"
    "src/main.cpp"
)

# ── Main target ───────────────────────────────────────────────────────────────
add_executable(Skyscribe WIN32 ${SKYSCRIBE_APP_SOURCES})

target_include_directories(Skyscribe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(Skyscribe PRIVATE
    SkyscribeLib
    imgui::imgui
    nlohmann_json::nlohmann_json
    d3d11
    dxgi
    d3dcompiler
    shell32
    ole32
    comdlg32
)

target_compile_definitions(Skyscribe PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
