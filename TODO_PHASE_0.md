# Phase 0 — Workspace & Build Shell

**Goal:** Compilable project that opens a window, renders the 3-panel ImGui layout at ≥ 60 fps, and shuts down cleanly. No game logic — infrastructure only.

**Exit criteria (M0):** `Skyscribe.exe` launches, shows Palette / Graph Editor / Preview panels, and closes without error. `cmake --preset release` succeeds on a clean machine.

---

## Tasks

### 0.1 — CMakeLists.txt with vcpkg toolchain `P0`

- [ ] Create root `CMakeLists.txt` targeting C++17 minimum
- [ ] Add `cmake/presets/CMakePresets.json` with `release` and `debug` presets
  - `release` preset: `Release` build type, vcpkg toolchain file, output to `build/release/`
  - `debug` preset: `Debug` build type, output to `build/debug/`
- [ ] Set vcpkg toolchain via `CMAKE_TOOLCHAIN_FILE` (either env var `VCPKG_ROOT` or preset variable)
- [ ] Add `tests/CMakeLists.txt` linked via `add_subdirectory`
- [ ] Verify: `cmake --preset release && cmake --build build/release --config Release` produces `Skyscribe.exe` on a clean machine with no manual steps

**Files to create:**
```
CMakeLists.txt
CMakePresets.json
tests/CMakeLists.txt
```

---

### 0.2 — vcpkg manifest (`vcpkg.json`) `P0`

- [ ] Create `vcpkg.json` at project root:
```json
{
  "name": "skyscribe",
  "version-string": "0.1.0",
  "dependencies": [
    "imgui[docking-experimental,dx11-binding,win32-binding]",
    "nlohmann-json",
    "catch2"
  ]
}
```
- [ ] Create `vcpkg-configuration.json` pinning the vcpkg baseline
- [ ] Verify all dependencies resolve and compile with no manual intervention
- [ ] Confirm `catch2` is available in the test target (even if no tests exist yet)

**Files to create:**
```
vcpkg.json
vcpkg-configuration.json
```

---

### 0.3 — DX11 + ImGui main loop `P0`

- [ ] Create `src/main.cpp` as Win32 entry point (`WinMain`)
- [ ] Create `src/app/Application.h` / `Application.cpp`:
  - `Application::Init()` — creates Win32 window, initialises D3D11 device + swap chain, initialises ImGui with DX11 backend
  - `Application::Run()` — main message loop: `PeekMessage` → `TranslateMessage` → `DispatchMessage` → ImGui `NewFrame` → `Render` → `Present`
  - `Application::Shutdown()` — releases all D3D11 resources, calls `ImGui::DestroyContext()`
- [ ] Win32 window setup:
  - Class name: `"SkyscribeWnd"`
  - Default size: 1280 × 720, resizable
  - Handle `WM_SIZE` to resize swap chain buffers
  - Handle `WM_QUIT` / `WM_CLOSE` cleanly
- [ ] D3D11 device creation:
  - Feature level: `D3D_FEATURE_LEVEL_11_0` minimum
  - DXGI swap chain: double-buffered, `DXGI_FORMAT_R8G8B8A8_UNORM`
  - VSync: on by default (`SyncInterval = 1`)
- [ ] ImGui initialisation:
  - `ImGui::CreateContext()`
  - `ImGui_ImplWin32_Init(hwnd)`
  - `ImGui_ImplDX11_Init(device, context)`
  - Override `IniFilename` to `nullptr` (layout managed manually — see task 0.4)
- [ ] Acceptance: window opens, title bar shows `"Skyscribe"`, renders at ≥ 60 fps, closes cleanly on Alt+F4 or `×` button

**Files to create:**
```
src/main.cpp
src/app/Application.h
src/app/Application.cpp
src/app/Settings.h        (stub — populated in Phase 1)
src/app/Settings.cpp      (stub)
```

---

### 0.4 — ImGui Docking layout (3-panel) `P0`

- [ ] Create `src/ui/MainWindow.h` / `MainWindow.cpp`:
  - Owns the top-level ImGui dockspace
  - Calls `ImGui::DockSpaceOverViewport()` each frame to establish the main docking area
- [ ] Stub the three panels (empty ImGui windows, correct labels, dockable):
  - `src/ui/ToolPalettePanel.h/.cpp` — left panel, label `"Tool Palette"`
  - `src/ui/GraphEditorPanel.h/.cpp` — centre panel, label `"Graph Editor"`
  - `src/ui/PreviewPanel.h/.cpp` — right panel, label `"Preview"`
- [ ] Default layout built programmatically on first run (no `imgui.ini` exists yet):
  ```
  ┌─────────────────────────────────────────────────┐
  │  Tool Palette  │  Graph Editor  │  Preview       │
  │   (~20% w)     │   (~55% w)     │  (~25% w)      │
  └─────────────────────────────────────────────────┘
  ```
  Use `ImGui::DockBuilderSplitNode()` to split left → palette, then right remainder → split right → preview / graph.
- [ ] Layout persistence:
  - Override `ImGui::GetIO().IniFilename = nullptr`
  - On startup: `ImGui::LoadIniSettingsFromDisk(iniPath)` where `iniPath = %APPDATA%\Skyscribe\imgui.ini`
  - On shutdown: `ImGui::SaveIniSettingsToDisk(iniPath)`
- [ ] Layout corruption recovery:
  - After first `ImGui::NewFrame()`: check `ImGui::DockBuilderGetNode(mainDockspaceId) != nullptr` and that all three expected panels are docked
  - If invalid: call `ResetLayoutToDefault()` which rebuilds the canonical layout programmatically and overwrites `imgui.ini`
- [ ] Acceptance: all three panels visible and independently resizable/redockable; layout persists across restarts

**Files to create / extend:**
```
src/ui/MainWindow.h
src/ui/MainWindow.cpp
src/ui/ToolPalettePanel.h   (stub)
src/ui/ToolPalettePanel.cpp (stub)
src/ui/GraphEditorPanel.h   (stub)
src/ui/GraphEditorPanel.cpp (stub)
src/ui/PreviewPanel.h       (stub)
src/ui/PreviewPanel.cpp     (stub)
```

---

### 0.5 — Logger `P1`

- [ ] Create `src/app/Logger.h` / `Logger.cpp`:
  - Singleton or namespace with three macros: `LOG_INFO(msg)`, `LOG_WARN(msg)`, `LOG_ERR(msg)`
  - Each entry is timestamped: `[HH:MM:SS] [LEVEL] message`
  - Thread-safe: uses a mutex-guarded queue; UI thread drains the queue each frame
- [ ] Dual output:
  - **Disk:** `%APPDATA%\Skyscribe\skyscribe.log` — appended on each run; rotated if > 10 MB (rename to `skyscribe.log.old`, start fresh)
  - **In-app console:** a dockable `"Application Log"` panel (can be a stub `ImGui::Begin` in Phase 0; fully implemented in Phase 2)
- [ ] `Help → View Log File...` menu item opens `skyscribe.log` in the default text editor via `ShellExecuteW`
- [ ] Acceptance: `LOG_INFO("Skyscribe started")` appears in both the console panel and on disk at `%APPDATA%\Skyscribe\skyscribe.log`

**Files to create:**
```
src/app/Logger.h
src/app/Logger.cpp
```

---

## Folder Skeleton to Create

```
skyscribe/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── vcpkg-configuration.json
│
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.h
│   │   ├── Application.cpp
│   │   ├── Settings.h          ← stub
│   │   ├── Settings.cpp        ← stub
│   │   ├── Logger.h
│   │   └── Logger.cpp
│   └── ui/
│       ├── MainWindow.h
│       ├── MainWindow.cpp
│       ├── ToolPalettePanel.h  ← stub
│       ├── ToolPalettePanel.cpp
│       ├── GraphEditorPanel.h  ← stub
│       ├── GraphEditorPanel.cpp
│       ├── PreviewPanel.h      ← stub
│       └── PreviewPanel.cpp
│
├── tests/
│   └── CMakeLists.txt          ← Catch2 test harness (no tests yet)
│
├── assets/
│   └── fonts/                  ← placeholder
│
└── third_party/                ← add imgui-node-editor as git submodule
    ├── imgui/                  ← via vcpkg
    ├── imgui-node-editor/      ← git submodule: thedmd/imgui-node-editor
    └── ImGuiColorTextEdit/     ← git submodule: santaclose/ImGuiColorTextEdit
```

---

## Definition of Done

- [ ] `cmake --preset release` completes without errors on a machine with only vcpkg + MSVC installed
- [ ] `Skyscribe.exe` opens a 1280×720 window titled `"Skyscribe"`
- [ ] Three panels (Tool Palette, Graph Editor, Preview) visible and resizable
- [ ] Framerate ≥ 60 fps (check via ImGui overlay or frame delta log)
- [ ] Window closes cleanly (no crash, no memory leak reported by MSVC debug heap)
- [ ] `LOG_INFO("Skyscribe started")` appears in `%APPDATA%\Skyscribe\skyscribe.log`
- [ ] Layout persists correctly after closing and reopening the application

---

## References

- ROADMAP §4 — C++ Project Structure
- ROADMAP §4.1 — Project Structure Addendum
- ROADMAP §6 — Milestone Overview (M0)
- ROADMAP §7 / Phase 0 tasks 0.1–0.5
- ROADMAP §39 — Window Layout & Docking Persistence
- ROADMAP §100 — Status Bar Layout (stub the widget in Phase 0, implement in Phase 2)
