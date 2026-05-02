cmake_minimum_required(VERSION 3.21)
project(Skyscribe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── vcpkg packages ────────────────────────────────────────────────────────────
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ── imgui-node-editor (vendored submodule) ────────────────────────────────────
add_library(imgui_node_editor STATIC
    third_party/imgui-node-editor/imgui_node_editor.cpp
    third_party/imgui-node-editor/imgui_node_editor_api.cpp
    third_party/imgui-node-editor/imgui_canvas.cpp
    third_party/imgui-node-editor/crude_json.cpp
)
target_include_directories(imgui_node_editor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-node-editor"
)
target_link_libraries(imgui_node_editor PUBLIC imgui::imgui)
target_compile_definitions(imgui_node_editor PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

# ── ImGuiColorTextEdit (vendored) ─────────────────────────────────────────────
add_library(imgui_color_text_edit STATIC
    third_party/ImGuiColorTextEdit/TextEditor.cpp
)
target_include_directories(imgui_color_text_edit PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit"
)
target_link_libraries(imgui_color_text_edit PUBLIC imgui::imgui)
target_compile_definitions(imgui_color_text_edit PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

# ── Shared logic library (compiler + project — used by main exe and tests) ────
file(GLOB_RECURSE SKYSCRIBE_LIB_SOURCES
    "src/compiler/*.cpp" "src/compiler/*.h"
    "src/project/*.cpp"  "src/project/*.h"
    "src/graph/*.cpp"    "src/graph/*.h"
    "src/codegen/*.cpp"  "src/codegen/*.h"
)
add_library(SkyscribeLib STATIC ${SKYSCRIBE_LIB_SOURCES})
target_include_directories(SkyscribeLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(SkyscribeLib PUBLIC nlohmann_json::nlohmann_json imgui_node_editor imgui_color_text_edit)
target_compile_definitions(SkyscribeLib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
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
    dwmapi
)

target_compile_definitions(Skyscribe PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

# ── Post-build: copy templates next to the executable ─────────────────────────
add_custom_command(TARGET Skyscribe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/templates"
        "$<TARGET_FILE_DIR:Skyscribe>/templates"
    COMMENT "Copying templates to output directory"
)
