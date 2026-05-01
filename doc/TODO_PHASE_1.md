# Phase 1 â€” Papyrus Compiler Pipeline

**Goal:** Hook the Creation Kit's `PapyrusCompiler.exe` into Skyscribe. A user can point the tool at their CK installation, open a script file, and compile it â€” seeing structured output (errors with file + line, warnings, success) inside the application. No visual graph yet; this phase is pure compiler plumbing, settings, and project lifecycle.

**Exit criteria (M1):** A hardcoded `Event OnInit()` Papyrus script compiles to `.pex` via **Compile â†’ Build** without leaving the application. Errors appear in the Output Panel with file + line number. Settings survive a restart. Project files save and load cleanly.

---

## Tasks

### 1.1 â€” Settings UI (paths + validation) `P0`

- [x] Create `src/app/Settings.h` / `Settings.cpp` (replace Phase 0 stubs):
  - Fields: `ck_compiler_path`, `ck_data_path`, `output_dir`, `import_dirs` (vector), `autosave_interval_s`
  - Serialised to / from `%APPDATA%\Skyscribe\config.json` via nlohmann/json
  - `Settings::Load()` â€” reads config, applies defaults for missing keys
  - `Settings::Save()` â€” atomic write (write to `.tmp`, rename to `.json`)
  - `Settings::Validate()` â€” returns a list of validation errors; each field reports independently
- [x] Open Settings via **Edit â†’ Settingsâ€¦** (or `Ctrl+,`)
- [x] Settings modal (`ImGui::BeginPopupModal`), tabbed layout:
  - **Compiler** tab â€” `PapyrusCompiler.exe` path (browse button + inline validation icon); CK Data path; Output directory
  - **Paths** tab â€” Import directories list: add / remove / reorder rows
  - **Autosave** tab â€” enable toggle + interval spinner (seconds)
- [x] Path validation: `std::filesystem::exists()` check on every field; invalid paths shown in red with a tooltip
- [x] Browse buttons open a Win32 folder/file picker (`SHBrowseForFolder` / `GetOpenFileName`)
- [x] Settings saved when user clicks **OK**; discarded on **Cancel**
- [x] Acceptance: paths persisted to `%APPDATA%\Skyscribe\config.json`; invalid paths shown in red

**Files to create / modify:**
```
src/app/Settings.h          â† replace stub
src/app/Settings.cpp        â† replace stub
src/ui/SettingsModal.h
src/ui/SettingsModal.cpp
```

---

### 1.2 â€” `CompilerWrapper` class `P0`

- [x] Create `src/compiler/CompilerWrapper.h` / `CompilerWrapper.cpp`:
  - `CompilerWrapper::BuildArgs(script_path)` â€” assembles the full command line:
    - `PapyrusCompiler.exe <script.psc> -f=<flags_file> -i=<import1>;<import2>;â€¦ -o=<output_dir>`
  - `CompilerWrapper::Invoke(script_path, callback)` â€” launches process via `CreateProcess` with stdout/stderr redirected through an anonymous pipe:
    - Reads pipe on a background thread; calls `callback(line)` for each line of output
    - Checks exit code: `0` = success, `1` = compile error, any other = unexpected failure
    - Implements 60-second timeout; on timeout calls `TerminateProcess` and raises a modal
  - `CompilerWrapper::Cancel()` â€” terminates the in-flight compile process
  - `struct CompileResult { bool success; std::vector<CompilerLine> lines; int exit_code; }`
- [x] Flags file auto-detection: search for `TESV_Papyrus_Flags.flg` under the CK Data path
- [x] Import directory auto-discovery: scan `Data\Scripts\Source\` and any user-specified extras from Settings; deduplicate
- [x] Acceptance: `CompilerWrapper::Invoke` wraps `CreateProcess`; captures stdout/stderr; returns structured result

**Files to create:**
```
src/compiler/CompilerWrapper.h
src/compiler/CompilerWrapper.cpp
```

---

### 1.3 â€” Compiler output panel `P0`

- [x] Create `src/ui/OutputPanel.h` / `OutputPanel.cpp` (replaces the Preview stub or adds a new docked panel):
  - Dockable ImGui window, label `"Output"`
  - Displays `CompilerLine` entries in a scrollable list
  - Color-coded rows: errors â†’ red, warnings â†’ yellow, info â†’ default text colour, success â†’ green
  - Each error/warning row shows: `[ERROR] <filename>(<line>): <message>`
  - Clicking an error row navigates to the relevant script file (stub for Phase 1 â€” just logs the intent)
  - Filter dropdown: **All / Errors / Warnings / Info**
  - Auto-scroll to bottom on new output; scroll lock button (ðŸ“Œ) to disable auto-scroll
  - Fatal error modal: if compiler exits with code â‰  0 and â‰  1, show `ImGui::OpenPopup("CompilerFatalError")`
- [x] **[Cancel]** button visible in the panel during an active compile; calls `CompilerWrapper::Cancel()`
- [x] Output cleared on each new compile invocation
- [x] Acceptance: errors shown with script name + line number; panel visible in the default layout

**Files to create:**
```
src/ui/OutputPanel.h
src/ui/OutputPanel.cpp
```

---

### 1.4 â€” Compiler output parsing `P0`

- [x] Create `src/compiler/CompilerLine.h`:
  ```cpp
  enum class LineKind { Info, Warning, Error, Success };
  struct CompilerLine {
      LineKind kind;
      std::string file;   // empty if not applicable
      int line_number;    // 0 if not applicable
      std::string text;   // full raw line
  };
  ```
- [x] Create `src/compiler/OutputParser.h` / `OutputParser.cpp`:
  - `OutputParser::Parse(raw_line) â†’ CompilerLine`
  - Regex rules (in priority order):
    1. `^(.+)\((\d+),\d+\): error: (.+)$` â†’ `LineKind::Error`
    2. `^(.+)\((\d+),\d+\): warning: (.+)$` â†’ `LineKind::Warning`
    3. `^Assembly of .+ succeeded$` â†’ `LineKind::Success`
    4. Anything else â†’ `LineKind::Info`
- [x] Unit tests in `tests/test_output_parser.cpp` (Catch2):
  - Error line parses correctly (file, line number, message)
  - Warning line parses correctly
  - Success line detected
  - Unknown line classified as `Info`
- [x] Acceptance: `OutputParser::Parse` correctly classifies all four line types

**Files to create:**
```
src/compiler/CompilerLine.h
src/compiler/OutputParser.h
src/compiler/OutputParser.cpp
tests/test_output_parser.cpp
```

---

### 1.5 â€” "Hello World" compile test `P0`

- [x] Add **Compile â†’ Build** menu item (also `F7` shortcut)
- [x] On invoke: run `CompilerWrapper::Invoke` on the active script path (Phase 1: hardcode a test `.psc` if no project is open)
- [x] Ship a minimal test script at `tests/scripts/HelloWorld.psc`:
  ```papyrus
  ScriptName HelloWorld

  Event OnInit()
    Debug.Notification("Hello from Skyscribe")
  EndEvent
  ```
- [x] Acceptance: `HelloWorld.psc` compiles to `HelloWorld.pex` in the configured output directory; output appears in the Output Panel

**Files to create:**
```
tests/scripts/HelloWorld.psc
```

---

### 1.6 â€” Missing CK error handling `P1`

- [x] If `PapyrusCompiler.exe` is not found at the configured path on compile invocation:
  - Show an `ImGui::OpenPopup("NoCKFound")` modal: _"Creation Kit compiler not found. Please configure the path in Edit â†’ Settings."_
  - Provide a **[Open Settings]** button in the modal
  - Provide a **[Download CK]** button that opens `https://store.steampowered.com/app/1946180` via `ShellExecuteW`
- [x] Acceptance: graceful message + link to CK download if exe not found

---

### 1.7 â€” Project lifecycle (New / Open / Save / Close) `P0`

- [x] Define `src/project/Project.h` / `Project.cpp`:
  - `struct ProjectMeta { std::string name; std::string root_dir; std::string created_at; std::string skyscribe_version; }`
  - `.skyscribe` project file format (JSON):
    ```json
    {
      "meta": { "name": "...", "root_dir": "...", "created_at": "...", "skyscribe_version": "0.1.0" },
      "graphs": [],
      "settings_override": {}
    }
    ```
  - `Project::New(name, dir)` â€” creates directory structure, writes blank `.skyscribe` file
  - `Project::Open(path)` â€” three-tier fallback: exact path â†’ scan dir for `.skyscribe` â†’ prompt user
  - `Project::Save()` â€” atomic write (`.tmp` â†’ rename)
  - `Project::Close()` â€” checks dirty flag, prompts if unsaved changes
- [x] **File → New Project** opens a dialog:
  - Fields: project name, parent directory (browse), template picker (see task 1.8)
  - Creates `<parent>/<name>/` and writes the `.skyscribe` file
- [x] **File → Open Project** (`Ctrl+O`) — Win32 file picker filtered to `*.skyscribe`
- [x] **File → Save** (`Ctrl+S`) — saves current project
- [x] **File → Save As…** — Win32 save-file picker; updates project root path
- [x] **File → Close Project** — unloads current project; returns to empty state
- [x] Recent projects list (last 10) persisted in `config.json`; shown in **File → Recent Projects** submenu
- [x] Dirty flag: title bar shows `Skyscribe — <project name> *` when there are unsaved changes
- [x] Unsaved-changes prompt on **Close / Open / New** when dirty flag is set
- [x] Acceptance: project files save and load; dirty flag and title bar indicator work

**Files to create:**
```
src/project/Project.h
src/project/Project.cpp
src/ui/NewProjectDialog.h
src/ui/NewProjectDialog.cpp
```

---

### 1.8 â€” New Project template system `P1`

- [x] `src/project/TemplateRegistry.h` / `TemplateRegistry.cpp`:
  - Scans `<exe_dir>\templates\` for subdirectories; each is a project template
  - Each template directory contains a `template.json` manifest (name, description, files to copy)
- [x] Ship three built-in templates under `templates/`:
  - `Blank` â€" empty `.skyscribe` file only
  - `QuestMod` â€" starter Quest script stub
  - `ActorAlias` â€" starter alias script stub
- [x] New Project dialog shows a template list panel with description text
- [x] Acceptance: all three templates appear in the New Project dialog; selecting one copies the correct stubs

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

### 1.9 â€” Window layout & DPI persistence `P1`

- [x] Persist window rect (position + size) in `config.json` under `"window": { "x", "y", "w", "h" }`
- [x] On startup: restore saved rect; clamp to available monitor work area to avoid off-screen placement
- [x] DPI awareness:
  - Set `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` via `SetProcessDpiAwarenessContext`
  - Handle `WM_DPICHANGED`: update window rect from `lParam`, scale ImGui font size
- [x] Layout validity check already in `MainWindow::Render()` (Phase 0); add auto-reset if `ImGui::DockBuilderGetNode` returns null
- [x] Acceptance: window position/size restores across restarts; DPI changes don't corrupt the layout

---

### 1.10 â€” Help menu & About dialog `P1`

- [x] **Help â†' Keyboard Shortcuts** â€" modal listing all shortcuts (`ImGui::BeginTable`, two columns: action / key)
- [x] **Help â†' About Skyscribe** â€" modal showing version string, build date, GPL v3 notice, GitHub link
- [x] **Help â View Log Fileâ¦** â already wired in Phase 0; verify it works with the updated Logger
- [x] **Help â†' Report a Bug** â€" `ShellExecuteW` to open GitHub issues URL
- [x] Acceptance: all four Help items functional

---

### 1.11 â€” Type registry stub `P1`

- [x] Create `src/graph/PinType.h`:
  ```cpp
  enum class PinType {
      Exec, Bool, Int, Float, String,
      ObjectRef, Actor, Quest, Form,
      Array_Bool, Array_Int, Array_Float, Array_String,
      Array_ObjectRef, Unknown
  };
  ```
- [x] `src/graph/PinColorMap.h` â€” `ImVec4 PinColor(PinType)` mapping (used in Phase 2 node editor)
- [x] `PinType::IsCompatible(PinType from, PinType to) â†’ bool` â€” basic subtype check (e.g. `Actor` â†’ `ObjectRef` is valid)
- [x] `None` literal: unconnected object-type output pins emit `None` in generated code

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
â”œâ”€â”€ compiler/
â”‚   â”œâ”€â”€ CompilerWrapper.h
â”‚   â”œâ”€â”€ CompilerWrapper.cpp
â”‚   â”œâ”€â”€ CompilerLine.h
â”‚   â”œâ”€â”€ OutputParser.h
â”‚   â””â”€â”€ OutputParser.cpp
â”œâ”€â”€ project/
â”‚   â”œâ”€â”€ Project.h
â”‚   â”œâ”€â”€ Project.cpp
â”‚   â”œâ”€â”€ TemplateRegistry.h
â”‚   â””â”€â”€ TemplateRegistry.cpp
â”œâ”€â”€ graph/
â”‚   â”œâ”€â”€ PinType.h
â”‚   â”œâ”€â”€ PinColorMap.h
â”‚   â””â”€â”€ PinColorMap.cpp
â””â”€â”€ ui/
    â”œâ”€â”€ OutputPanel.h
    â”œâ”€â”€ OutputPanel.cpp
    â”œâ”€â”€ SettingsModal.h
    â”œâ”€â”€ SettingsModal.cpp
    â”œâ”€â”€ NewProjectDialog.h
    â””â”€â”€ NewProjectDialog.cpp

templates/
â”œâ”€â”€ Blank/template.json
â”œâ”€â”€ QuestMod/template.json
â”œâ”€â”€ QuestMod/QuestScript.psc
â”œâ”€â”€ ActorAlias/template.json
â””â”€â”€ ActorAlias/AliasScript.psc

tests/
â”œâ”€â”€ scripts/HelloWorld.psc
â””â”€â”€ test_output_parser.cpp
```

---

## Definition of Done

- [ ] `cmake --preset release` builds with zero errors and zero warnings
- [ ] `HelloWorld.psc` compiles to `.pex` via **Compile â†’ Build** (`F7`) without leaving the application
- [ ] Compiler errors appear in the Output Panel with file name + line number, colour-coded
- [ ] `OutputParser` unit tests pass (`ctest --preset release`)
- [ ] Settings (compiler path, import dirs, autosave) persist across restarts via `config.json`
- [ ] **New / Open / Save / Close** project lifecycle works; dirty flag shown in title bar
- [ ] Missing CK shows a modal with **[Open Settings]** + **[Download CK]** buttons
- [ ] Window position/size restores after restart; DPI change does not corrupt layout
- [ ] `LOG_INFO("Compile started")` and `LOG_INFO("Compile finished: <result>")` appear in `skyscribe.log`
