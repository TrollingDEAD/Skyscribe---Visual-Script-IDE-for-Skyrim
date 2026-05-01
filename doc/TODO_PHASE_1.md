# Phase 1 — Papyrus Compiler Pipeline

**Goal:** Hook the Creation Kit's `PapyrusCompiler.exe` into Skyscribe. A user can point the tool at their CK installation, open a script file, and compile it — seeing structured output (errors with file + line, warnings, success) inside the application. No visual graph yet; this phase is pure compiler plumbing, settings, and project lifecycle.

**Exit criteria (M1):** A hardcoded `Event OnInit()` Papyrus script compiles to `.pex` via **Compile → Build** without leaving the application. Errors appear in the Output Panel with file + line number. Settings survive a restart. Project files save and load cleanly.

---

## Tasks

### 1.1 — Settings UI (paths + validation) `P0`

- [ ] Create `src/app/Settings.h` / `Settings.cpp` (replace Phase 0 stubs):
  - Fields: `ck_compiler_path`, `ck_data_path`, `output_dir`, `import_dirs` (vector), `autosave_interval_s`
  - Serialised to / from `%APPDATA%\Skyscribe\config.json` via nlohmann/json
  - `Settings::Load()` — reads config, applies defaults for missing keys
  - `Settings::Save()` — atomic write (write to `.tmp`, rename to `.json`)
  - `Settings::Validate()` — returns a list of validation errors; each field reports independently
- [ ] Open Settings via **Edit → Settings…** (or `Ctrl+,`)
- [ ] Settings modal (`ImGui::BeginPopupModal`), tabbed layout:
  - **Compiler** tab — `PapyrusCompiler.exe` path (browse button + inline validation icon); CK Data path; Output directory
  - **Paths** tab — Import directories list: add / remove / reorder rows
  - **Autosave** tab — enable toggle + interval spinner (seconds)
- [ ] Path validation: `std::filesystem::exists()` check on every field; invalid paths shown in red with a tooltip
- [ ] Browse buttons open a Win32 folder/file picker (`SHBrowseForFolder` / `GetOpenFileName`)
- [ ] Settings saved when user clicks **OK**; discarded on **Cancel**
- [ ] Acceptance: paths persisted to `%APPDATA%\Skyscribe\config.json`; invalid paths shown in red

**Files to create / modify:**
```
src/app/Settings.h          ← replace stub
src/app/Settings.cpp        ← replace stub
src/ui/SettingsModal.h
src/ui/SettingsModal.cpp
```

---

### 1.2 — `CompilerWrapper` class `P0`

- [ ] Create `src/compiler/CompilerWrapper.h` / `CompilerWrapper.cpp`:
  - `CompilerWrapper::BuildArgs(script_path)` — assembles the full command line:
    - `PapyrusCompiler.exe <script.psc> -f=<flags_file> -i=<import1>;<import2>;… -o=<output_dir>`
  - `CompilerWrapper::Invoke(script_path, callback)` — launches process via `CreateProcess` with stdout/stderr redirected through an anonymous pipe:
    - Reads pipe on a background thread; calls `callback(line)` for each line of output
    - Checks exit code: `0` = success, `1` = compile error, any other = unexpected failure
    - Implements 60-second timeout; on timeout calls `TerminateProcess` and raises a modal
  - `CompilerWrapper::Cancel()` — terminates the in-flight compile process
  - `struct CompileResult { bool success; std::vector<CompilerLine> lines; int exit_code; }`
- [ ] Flags file auto-detection: search for `TESV_Papyrus_Flags.flg` under the CK Data path
- [ ] Import directory auto-discovery: scan `Data\Scripts\Source\` and any user-specified extras from Settings; deduplicate
- [ ] Acceptance: `CompilerWrapper::Invoke` wraps `CreateProcess`; captures stdout/stderr; returns structured result

**Files to create:**
```
src/compiler/CompilerWrapper.h
src/compiler/CompilerWrapper.cpp
```

---

### 1.3 — Compiler output panel `P0`

- [ ] Create `src/ui/OutputPanel.h` / `OutputPanel.cpp` (replaces the Preview stub or adds a new docked panel):
  - Dockable ImGui window, label `"Output"`
  - Displays `CompilerLine` entries in a scrollable list
  - Color-coded rows: errors → red, warnings → yellow, info → default text colour, success → green
  - Each error/warning row shows: `[ERROR] <filename>(<line>): <message>`
  - Clicking an error row navigates to the relevant script file (stub for Phase 1 — just logs the intent)
  - Filter dropdown: **All / Errors / Warnings / Info**
  - Auto-scroll to bottom on new output; scroll lock button (📌) to disable auto-scroll
  - Fatal error modal: if compiler exits with code ≠ 0 and ≠ 1, show `ImGui::OpenPopup("CompilerFatalError")`
- [ ] **[Cancel]** button visible in the panel during an active compile; calls `CompilerWrapper::Cancel()`
- [ ] Output cleared on each new compile invocation
- [ ] Acceptance: errors shown with script name + line number; panel visible in the default layout

**Files to create:**
```
src/ui/OutputPanel.h
src/ui/OutputPanel.cpp
```

---

### 1.4 — Compiler output parsing `P0`

- [ ] Create `src/compiler/CompilerLine.h`:
  ```cpp
  enum class LineKind { Info, Warning, Error, Success };
  struct CompilerLine {
      LineKind kind;
      std::string file;   // empty if not applicable
      int line_number;    // 0 if not applicable
      std::string text;   // full raw line
  };
  ```
- [ ] Create `src/compiler/OutputParser.h` / `OutputParser.cpp`:
  - `OutputParser::Parse(raw_line) → CompilerLine`
  - Regex rules (in priority order):
    1. `^(.+)\((\d+),\d+\): error: (.+)$` → `LineKind::Error`
    2. `^(.+)\((\d+),\d+\): warning: (.+)$` → `LineKind::Warning`
    3. `^Assembly of .+ succeeded$` → `LineKind::Success`
    4. Anything else → `LineKind::Info`
- [ ] Unit tests in `tests/test_output_parser.cpp` (Catch2):
  - Error line parses correctly (file, line number, message)
  - Warning line parses correctly
  - Success line detected
  - Unknown line classified as `Info`
- [ ] Acceptance: `OutputParser::Parse` correctly classifies all four line types

**Files to create:**
```
src/compiler/CompilerLine.h
src/compiler/OutputParser.h
src/compiler/OutputParser.cpp
tests/test_output_parser.cpp
```

---

### 1.5 — "Hello World" compile test `P0`

- [ ] Add **Compile → Build** menu item (also `F7` shortcut)
- [ ] On invoke: run `CompilerWrapper::Invoke` on the active script path (Phase 1: hardcode a test `.psc` if no project is open)
- [ ] Ship a minimal test script at `tests/scripts/HelloWorld.psc`:
  ```papyrus
  ScriptName HelloWorld

  Event OnInit()
    Debug.Notification("Hello from Skyscribe")
  EndEvent
  ```
- [ ] Acceptance: `HelloWorld.psc` compiles to `HelloWorld.pex` in the configured output directory; output appears in the Output Panel

**Files to create:**
```
tests/scripts/HelloWorld.psc
```

---

### 1.6 — Missing CK error handling `P1`

- [ ] If `PapyrusCompiler.exe` is not found at the configured path on compile invocation:
  - Show an `ImGui::OpenPopup("NoCKFound")` modal: _"Creation Kit compiler not found. Please configure the path in Edit → Settings."_
  - Provide a **[Open Settings]** button in the modal
  - Provide a **[Download CK]** button that opens `https://store.steampowered.com/app/1946180` via `ShellExecuteW`
- [ ] Acceptance: graceful message + link to CK download if exe not found

---

### 1.7 — Project lifecycle (New / Open / Save / Close) `P0`

- [ ] Define `src/project/Project.h` / `Project.cpp`:
  - `struct ProjectMeta { std::string name; std::string root_dir; std::string created_at; std::string skyscribe_version; }`
  - `.skyscribe` project file format (JSON):
    ```json
    {
      "meta": { "name": "...", "root_dir": "...", "created_at": "...", "skyscribe_version": "0.1.0" },
      "graphs": [],
      "settings_override": {}
    }
    ```
  - `Project::New(name, dir)` — creates directory structure, writes blank `.skyscribe` file
  - `Project::Open(path)` — three-tier fallback: exact path → scan dir for `.skyscribe` → prompt user
  - `Project::Save()` — atomic write (`.tmp` → rename)
  - `Project::Close()` — checks dirty flag, prompts if unsaved changes
- [ ] **File → New Project** opens a dialog:
  - Fields: project name, parent directory (browse), template picker (see task 1.9)
  - Creates `<parent>/<name>/` and writes the `.skyscribe` file
- [ ] **File → Open Project** (`Ctrl+O`) — Win32 file picker filtered to `*.skyscribe`
- [ ] **File → Save** (`Ctrl+S`) — saves current project
- [ ] **File → Save As…** — Win32 save-file picker; updates project root path
- [ ] **File → Close Project** — unloads current project; returns to empty state
- [ ] Recent projects list (last 10) persisted in `config.json`; shown in **File → Recent Projects** submenu
- [ ] Dirty flag: title bar shows `Skyscribe — <project name> *` when there are unsaved changes
- [ ] Unsaved-changes prompt on **Close / Open / New** when dirty flag is set
- [ ] Acceptance: project files save and load; dirty flag and title bar indicator work

**Files to create:**
```
src/project/Project.h
src/project/Project.cpp
src/ui/NewProjectDialog.h
src/ui/NewProjectDialog.cpp
```

---

### 1.8 — New Project template system `P1`

- [ ] `src/project/TemplateRegistry.h` / `TemplateRegistry.cpp`:
  - Scans `<exe_dir>\templates\` for subdirectories; each is a project template
  - Each template directory contains a `template.json` manifest (name, description, files to copy)
- [ ] Ship three built-in templates under `templates/`:
  - `Blank` — empty `.skyscribe` file only
  - `QuestMod` — starter Quest script stub
  - `ActorAlias` — starter alias script stub
- [ ] New Project dialog shows a template list panel with description text
- [ ] Acceptance: all three templates appear in the New Project dialog; selecting one copies the correct stubs

**Files to create:**
```
src/project/TemplateRegistry.h
src/project/TemplateRegistry.cpp
templates/Blank/template.json
templates/QuestMod/template.json
templates/QuestMod/QuestScript.psc
templates/ActorAlias/template.json
templates/ActorAlias/AliasScript.psc
```

---

### 1.9 — Window layout & DPI persistence `P1`

- [ ] Persist window rect (position + size) in `config.json` under `"window": { "x", "y", "w", "h" }`
- [ ] On startup: restore saved rect; clamp to available monitor work area to avoid off-screen placement
- [ ] DPI awareness:
  - Set `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` via `SetProcessDpiAwarenessContext`
  - Handle `WM_DPICHANGED`: update window rect from `lParam`, scale ImGui font size
- [ ] Layout validity check already in `MainWindow::Render()` (Phase 0); add auto-reset if `ImGui::DockBuilderGetNode` returns null
- [ ] Acceptance: window position/size restores across restarts; DPI changes don't corrupt the layout

---

### 1.10 — Help menu & About dialog `P1`

- [ ] **Help → Keyboard Shortcuts** — modal listing all shortcuts (`ImGui::BeginTable`, two columns: action / key)
- [ ] **Help → About Skyscribe** — modal showing version string, build date, GPL v3 notice, GitHub link
- [ ] **Help → View Log File…** — already wired in Phase 0; verify it works with the updated Logger
- [ ] **Help → Report a Bug** — `ShellExecuteW` to open GitHub issues URL
- [ ] Acceptance: all four Help items functional

---

### 1.11 — Type registry stub `P1`

- [ ] Create `src/graph/PinType.h`:
  ```cpp
  enum class PinType {
      Exec, Bool, Int, Float, String,
      ObjectRef, Actor, Quest, Form,
      Array_Bool, Array_Int, Array_Float, Array_String,
      Array_ObjectRef, Unknown
  };
  ```
- [ ] `src/graph/PinColorMap.h` — `ImVec4 PinColor(PinType)` mapping (used in Phase 2 node editor)
- [ ] `PinType::IsCompatible(PinType from, PinType to) → bool` — basic subtype check (e.g. `Actor` → `ObjectRef` is valid)
- [ ] `None` literal: unconnected object-type output pins emit `None` in generated code

**Files to create:**
```
src/graph/PinType.h
src/graph/PinColorMap.h
src/graph/PinColorMap.cpp
```

---

## Folder Additions

```
src/
├── compiler/
│   ├── CompilerWrapper.h
│   ├── CompilerWrapper.cpp
│   ├── CompilerLine.h
│   ├── OutputParser.h
│   └── OutputParser.cpp
├── project/
│   ├── Project.h
│   ├── Project.cpp
│   ├── TemplateRegistry.h
│   └── TemplateRegistry.cpp
├── graph/
│   ├── PinType.h
│   ├── PinColorMap.h
│   └── PinColorMap.cpp
└── ui/
    ├── OutputPanel.h
    ├── OutputPanel.cpp
    ├── SettingsModal.h
    ├── SettingsModal.cpp
    ├── NewProjectDialog.h
    └── NewProjectDialog.cpp

templates/
├── Blank/template.json
├── QuestMod/template.json
├── QuestMod/QuestScript.psc
├── ActorAlias/template.json
└── ActorAlias/AliasScript.psc

tests/
├── scripts/HelloWorld.psc
└── test_output_parser.cpp
```

---

## Definition of Done

- [ ] `cmake --preset release` builds with zero errors and zero warnings
- [ ] `HelloWorld.psc` compiles to `.pex` via **Compile → Build** (`F7`) without leaving the application
- [ ] Compiler errors appear in the Output Panel with file name + line number, colour-coded
- [ ] `OutputParser` unit tests pass (`ctest --preset release`)
- [ ] Settings (compiler path, import dirs, autosave) persist across restarts via `config.json`
- [ ] **New / Open / Save / Close** project lifecycle works; dirty flag shown in title bar
- [ ] Missing CK shows a modal with **[Open Settings]** + **[Download CK]** buttons
- [ ] Window position/size restores after restart; DPI change does not corrupt layout
- [ ] `LOG_INFO("Compile started")` and `LOG_INFO("Compile finished: <result>")` appear in `skyscribe.log`
