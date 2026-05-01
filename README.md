# Skyscribe

**A native Visual Script IDE for Skyrim SE/AE Papyrus modding.**

Skyscribe lets you build Skyrim scripts using a Blueprint-style node graph — no Papyrus syntax required. It handles code generation, compilation, dependency tracking, runtime log tracing, and full mod packaging in a single desktop application.

> **Status:** Active development — pre-alpha. Not yet feature-complete.  
> **Platform:** Windows 10/11 only.

---

## Features

### Visual Scripting
- Blueprint-style node canvas powered by [imgui-node-editor](https://github.com/thedmd/imgui-node-editor)
- Execution-flow (exec) pins and typed data-flow pins with full Papyrus type safety
- Built-in node library: Events, Control Flow, Variables, Math & Logic, String ops, Utility nodes
- Live `.psc` preview updates as you edit the graph — teaches Papyrus while you build
- Per-tab undo/redo, multi-select, copy/paste, alignment tools, and canvas search

### Compiler Integration
- One-click compile via `PapyrusCompiler.exe` (bundled with the Creation Kit)
- Output panel with clickable error lines that jump to the offending node
- Compile All with parallelism across project scripts
- Build & Deploy: compile → mirror outputs directly into your Mod Organizer 2 mod folder

### Reflection Engine
- Import any `.psc` source file — it becomes a palette of usable nodes instantly
- Imports the full Skyrim vanilla source library, SKSE64 sources, and third-party mods
- SQLite metadata cache with SHA-256 fingerprinting; re-scan only on file changes
- Decompile `.pex` bytecode via [Champollion](https://github.com/Orvid/Champollion) — import as callable library or open as a read-only graph

### EditorID Catalog
- Import your xEdit JSON export to get a searchable form reference picker
- Auto-resolves EditorIDs to `(plugin, FormID)` tuples in codegen — no manual hex
- Disambiguation for duplicate EditorIDs across plugins with plugin-column sorting

### Data-Driven Framework Output (SPID / BOS / KID)
- Dedicated Distribution Spec canvas for [SPID](https://github.com/powerof3/Spell-Perk-Item-Distributor), [BOS](https://github.com/powerof3/Base-Object-Swapper), and [KID](https://github.com/powerof3/Keyword-Item-Distributor)
- Declarative filter nodes: By NPC, By Faction, By Keyword, By Level, By Plugin Present
- Emits valid `_DISTR.ini`, `_SWAP.ini`, and `_KID.ini` files as part of the standard build pipeline
- D01–D04 lint diagnostics with node-level error decorations

### Runtime Log Tracing
- Live-tails `Papyrus.0.log` from your Skyrim installation
- Project-only filter shows only lines relevant to your scripts
- Click any log line to navigate to the matching script tab and function
- Built-in Trace Node wraps `Debug.Trace` with optional `[ScriptName::Function]` prefix

### Mod Packaging
- One-click `.zip` archive with correct `Data/Scripts/` layout
- `.bsa` v105 packer for Skyrim SE/AE
- FOMOD generator: `ModuleConfig.xml` + `info.xml` for MO2/Vortex
- Dependency analyzer: identifies third-party nodes used and generates a requirements list

---

## Requirements

### Runtime
| Requirement | Details |
|-------------|---------|
| OS | Windows 10 or Windows 11 (64-bit) |
| GPU | DirectX 11 capable |
| [Creation Kit](https://store.steampowered.com/app/1946180/Skyrim_Special_Edition_Creation_Kit/) | Required — provides `PapyrusCompiler.exe` |
| Skyrim SE or AE | Required — `Data/` folder for source import |

### Optional
| Tool | Purpose |
|------|---------|
| [SKSE64](https://skse.silverlock.org/) | Reflect SKSE Papyrus extension sources |
| [Champollion](https://github.com/Orvid/Champollion) | Decompile `.pex` bytecode for import |
| [xEdit](https://github.com/TES5Edit/TES5Edit) | Export EditorID catalog JSON |

### Build
| Tool | Version |
|------|---------|
| CMake | ≥ 3.21 |
| vcpkg | latest |
| C++ compiler | MSVC 2022 (C++17) |
| Windows SDK | 10.0.19041+ |

---

## Building from Source

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/TrollingDEAD/Skyscribe---Visual-Script-IDE-for-Skyrim.git
cd Skyscribe---Visual-Script-IDE-for-Skyrim
```

### 2. Install vcpkg (if not already present)

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### 3. Configure and build

```powershell
cmake --preset release
cmake --build build/release --config Release
```

The binary is output to `build/release/Release/Skyscribe.exe`.

> **Note:** The first build will take several minutes while vcpkg fetches and compiles dependencies (ImGui, nlohmann-json, Flatbuffers, SQLite, libarchive, Catch2).

### 4. Run tests

```powershell
cd build/release
ctest -C Release --output-on-failure
```

---

## First-Run Setup

On first launch, the **Setup Wizard** will prompt for:

1. **Skyrim Data folder** — auto-detected from the Steam registry; browse manually if not found
2. **Creation Kit path** — auto-detected from the same Skyrim install root
3. **SKSE64 path** — optional; skip if not installed

Settings are stored in `%APPDATA%\Skyscribe\config.json`. Re-run the wizard at any time via **Help → Re-run Setup Wizard**.

---

## Project Files

| File / Folder | Description |
|---------------|-------------|
| `MyMod.skyscribe` | Binary project file (Flatbuffers); primary save format |
| `MyMod.skyscribe.json` | VCS-friendly JSON mirror; auto-generated on every save |
| `metadata.db` | SQLite reflection cache; safe to delete (will be rebuilt) |
| `.skyscribe_tmp/` | Temporary decompile artifacts; deleted on clean close |
| `.skyscribe_deploy.json` | Deployment manifest for Build & Deploy ownership tracking |

Add `metadata.db` and `.skyscribe_tmp/` to your `.gitignore`. Commit the `.skyscribe.json` mirror for version control — the binary `.skyscribe` is optional.

---

## Tech Stack

| Component | Library |
|-----------|---------|
| UI Framework | [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) |
| Node Editor | [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) |
| Code Editor | [ImGuiColorTextEdit](https://github.com/santaclose/ImGuiColorTextEdit) |
| Graphics | DirectX 11 |
| Serialization | [Flatbuffers](https://github.com/google/flatbuffers) + [nlohmann/json](https://github.com/nlohmann/json) |
| Metadata DB | [SQLite](https://sqlite.org/) ≥ 3.45 |
| Archiving | [libarchive](https://github.com/libarchive/libarchive) |
| Testing | [Catch2](https://github.com/catchorg/Catch2) v3 |
| Build | CMake ≥ 3.21 + vcpkg |
| Language | C++17 |

---

## Project Structure

```
skyscribe/
├── CMakeLists.txt
├── vcpkg.json
│
├── src/
│   ├── main.cpp
│   ├── app/            # Application, Settings, Logger
│   ├── ui/             # All ImGui panels and dialogs
│   ├── graph/          # ScriptGraph, nodes, pins, connections, undo stack
│   ├── codegen/        # Graph traversal, Papyrus string builder, INI emitters
│   ├── parser/         # .psc lexer, AST parser, type registry
│   ├── reflection/     # Reflection engine, SQLite cache, EditorID catalog
│   ├── compiler/       # PapyrusCompiler and Champollion wrappers
│   ├── packaging/      # VFS tracker, dependency analyzer, archive/BSA/FOMOD, deploy
│   └── serialization/  # Flatbuffers project r/w, JSON mirror
│
├── tests/
│   ├── test_codegen.cpp
│   ├── test_parser.cpp
│   ├── test_type_registry.cpp
│   ├── test_graph_serialization.cpp
│   ├── test_packaging.cpp
│   ├── test_champollion.cpp
│   ├── test_editor_id_catalog.cpp
│   ├── test_log_source_map.cpp
│   ├── test_build_deploy.cpp
│   └── test_distribution_spec.cpp
│
├── assets/
│   └── fonts/
│
└── third_party/
    ├── imgui/
    ├── imgui-node-editor/
    └── ImGuiColorTextEdit/
```

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full planning document (~7,400 lines) covering all 111 design sections, phase task breakdowns, risk register, and glossary.

### Milestone summary

| Milestone | Status | Exit Criteria |
|-----------|--------|---------------|
| M0 — Workspace & Build Shell | Planned | App window opens; DX11 + ImGui ≥ 60 fps |
| M1 — Compiler Integration | Planned | `.psc` → `.pex` round-trip in-app |
| M2 — Interactive Node Graph | Planned | Nodes drag; type-safe connections; save/load |
| M3 — Live Papyrus Preview | Planned | Graph change → `.psc` update in real time |
| M4 — Reflection Engine | Planned | Third-party `.psc` produces palette nodes |
| M5 — Pack & Ship + FOMOD | Planned | One-click `.zip` and FOMOD generation |
| M6 — Alpha Release | Planned | No P0 bugs; first-run wizard complete |

---

## Contributing

This project is in early design/pre-alpha. Contributions are welcome once a minimum viable build is published. In the meantime:

- **Bug reports and feature discussions:** open an Issue
- **Code contributions:** please open an Issue first to discuss scope before submitting a PR

---

## License

*License TBD — will be determined before the first public release.*

Third-party components are subject to their own licenses: MIT (ImGui, imgui-node-editor, ImGuiColorTextEdit, nlohmann/json, vcpkg), Apache-2.0 (Flatbuffers), BSD-2 (libarchive), BSL-1.0 (Catch2), Public Domain (SQLite).

---

## Legal Notice

Skyscribe is an independent community tool. It is not affiliated with, endorsed by, or produced by Bethesda Softworks or ZeniMax Media. *The Elder Scrolls V: Skyrim* is a registered trademark of ZeniMax Media Inc.

SPID, BOS, and KID are community mods by [powerof3](https://github.com/powerof3). Champollion is a community decompiler by [Orvid](https://github.com/Orvid/Champollion). Skyscribe merely invokes these tools; it does not distribute them.
