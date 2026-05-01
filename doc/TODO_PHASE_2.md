# Phase 2 — Graph System & Node Architecture

**Goal:** Implement the visual node graph that is the heart of Skyscribe. Users can place nodes from a categorised palette, connect pins with typed wires, and serialize the graph to/from the project file. No code generation yet — that is Phase 3. This phase delivers a fully interactive canvas with undo/redo, multi-script support, and the complete built-in node library.

**Exit criteria (M2):** A user can open a project, place an `OnInit` event node and a `Debug.Notification` node, connect them, save, close, and reload — and the graph is identical. The canvas supports zoom/pan, rubber-band selection, copy/paste, and undo/redo. All built-in nodes (§8 of ROADMAP) appear in the palette.

---

## Prerequisites

The following Phase 1 outputs are consumed by Phase 2 and must remain stable:

- `src/graph/PinType.h` — `PinType` enum + `IsCompatible()` ✅
- `src/graph/PinColorMap.h` — `PinColor()` ✅
- `src/project/Project.h/.cpp` — project lifecycle (New/Open/Save) ✅
- `src/app/Settings.h/.cpp` — config persistence ✅

---

## Dependency: imgui-node-editor

imgui-node-editor (`thedmd/imgui-node-editor`) must be added to vcpkg before any canvas work begins.

- [ ] Add `"imgui-node-editor"` to `vcpkg.json`
- [ ] Verify `find_package` and `target_link_libraries` in `CMakeLists.txt`
- [ ] Confirm `#include <imgui_node_editor.h>` compiles in a trivial test file
- [ ] Acceptance: `cmake --preset release` succeeds with imgui-node-editor linked

---

## Tasks

### 2.1 — Core graph data model `P0`

> ROADMAP §18, §80

- [ ] Create `src/graph/NodeDefinition.h`:
  - `enum class PinKind { Input, Output }`
  - `enum class PinFlow { Execution, Data }`
  - `enum class PapyrusType` — mirrors `PinType` but used in definitions (can alias)
  - `struct PinDefinition { std::string name; PinKind kind; PinFlow flow; PinType type; std::string default_value; std::string tooltip; }`
  - `enum class NodeCategory { Event, ControlFlow, Variable, Math, Debug, Actor, Quest, Utility, Custom }`
  - `struct NodeDefinition { std::string type_id; std::string display_name; NodeCategory category; std::string tooltip; std::string source_script; std::vector<PinDefinition> pins; std::string codegen_template; }`
- [ ] Create `src/graph/ScriptGraph.h`:
  - `struct Pin { uint64_t id; std::string name; PinKind kind; PinFlow flow; PinType type; std::string value; }` — runtime pin instance
  - `struct ScriptNode { uint64_t id; std::string type_id; float pos_x, pos_y; std::vector<Pin> pins; }` — runtime node instance
  - `struct Connection { uint64_t id; uint64_t from_pin_id; uint64_t to_pin_id; }`
  - `struct ScriptGraph { std::string script_name; std::string extends; std::vector<ScriptNode> nodes; std::vector<Connection> connections; uint64_t next_node_id = 1; uint64_t next_pin_id = 1; uint64_t next_conn_id = 1; }`
  - Helper methods: `AddNode()`, `RemoveNode()`, `Connect()`, `Disconnect()`, `FindNode()`, `FindPin()`
- [ ] Create `src/graph/ScriptGraph.cpp` implementing the helpers
- [ ] **Unit tests** (`tests/test_graph_model.cpp`):
  - Add/remove node updates `next_node_id`; deleted IDs are not reused
  - `Connect` rejects incompatible types via `IsCompatible()`
  - `Connect` rejects Exec→Data and Data→Exec
  - `RemoveNode` also removes all connections to/from that node's pins
  - Round-trip: add 3 nodes, 2 connections, remove middle node → only 0 connections remain
- [ ] Acceptance: 5+ Catch2 tests pass; model is usable without UI

**Files to create:**
```
src/graph/NodeDefinition.h
src/graph/ScriptGraph.h
src/graph/ScriptGraph.cpp
tests/test_graph_model.cpp
```

---

### 2.2 — NodeRegistry & built-in node library `P0`

> ROADMAP §8, §18

- [ ] Create `src/graph/NodeRegistry.h` / `NodeRegistry.cpp`:
  - Singleton `NodeRegistry::Get()`
  - `Register(NodeDefinition)` — adds to internal map keyed by `type_id`
  - `Find(type_id) → const NodeDefinition*`
  - `AllNodes() → const std::vector<NodeDefinition>&` — sorted by category then display name
  - `ByCategory(NodeCategory) → std::vector<const NodeDefinition*>`
- [ ] Create `src/graph/BuiltinNodes.h` / `BuiltinNodes.cpp`:
  - `BuiltinNodes::RegisterAll()` — registers every node from ROADMAP §8 into `NodeRegistry`
  - **Event nodes** (12): `OnInit`, `OnActivate`, `OnLoad`, `OnUnload`, `OnDeath`, `OnHit`, `OnEquipped`, `OnUnequipped`, `OnCombatStateChanged`, `OnTimer`, `OnUpdateGameTime`, `OnStageSet`
  - **Control Flow nodes** (4): `If/Else`, `While Loop`, `Return`, `Sequence`
  - **Variable nodes** (8): `Set Variable`, `Get Variable`, `Declare Local`, `Literal Int`, `Literal Float`, `Literal String`, `Literal Bool`, `Cast`
  - **Math & Logic nodes** (14): `Add(Int)`, `Subtract(Int)`, `Multiply(Int)`, `Divide(Int)`, `Modulo`, `Add(Float)`, `Multiply(Float)`, `And`, `Or`, `Not`, `Equal`, `Not Equal`, `Greater Than`, `Less Than`
  - **Debug nodes** (3): `Notification`, `Message Box`, `Trace`
  - **Actor nodes** (9): `Get Player`, `Get Form By ID`, `Apply Spell`, `Remove Spell`, `Is Dead`, `Get Level`, `Resurrect`, `Get Actor Value`, `Set Actor Value`
  - **Quest/Story nodes** (5): `Set Stage`, `Get Stage`, `Is Stage Done`, `Complete Quest`, `Start Quest`
  - **Utility/Timer nodes** (6): `Start Timer`, `Cancel Timer`, `Start Timer (Game Time)`, `Wait`, `Random Int`, `Random Float`
- [ ] Call `BuiltinNodes::RegisterAll()` from `Application::Init()` before any UI renders
- [ ] **Unit tests** (`tests/test_node_registry.cpp`):
  - Total registered node count matches expected (61 built-ins)
  - `Find("builtin.OnInit")` returns non-null with correct `display_name` and `category`
  - `ByCategory(NodeCategory::Event)` returns exactly 12 nodes
  - `AllNodes()` is sorted: Events come before ControlFlow
- [ ] Acceptance: 4+ tests pass; all §8 nodes registered at startup

**Files to create:**
```
src/graph/NodeRegistry.h
src/graph/NodeRegistry.cpp
src/graph/BuiltinNodes.h
src/graph/BuiltinNodes.cpp
tests/test_node_registry.cpp
```

---

### 2.3 — Execution-flow and data-flow pin connection enforcement `P0`

> ROADMAP §2.2, §2.3, §80 (pin IDs)

- [ ] Pin IDs are computed deterministically: `pin_id = (node_id << 16) | pin_index` — fits in `uint64_t`; no separate counter needed
- [ ] `ScriptGraph::Connect()` enforces at the model level:
  - Both pins must exist (valid `pin_id`)
  - `from` pin must be `PinKind::Output`; `to` pin must be `PinKind::Input`
  - Both must have the same `PinFlow` (Execution↔Execution or Data↔Data)
  - For Data pins: `IsCompatible(from.type, to.type)` must return `true`
  - A Data input pin may only have **one** incoming connection (replace existing on reconnect)
  - An Execution output pin may fan-out to multiple inputs
  - Returns `bool` (false = rejected); logs rejection reason via `LOG_WARN`
- [ ] `ScriptGraph::CanConnect(from_pin_id, to_pin_id) → bool` — same checks without mutating state; used by UI for hover feedback
- [ ] **Unit tests** in `test_graph_model.cpp`:
  - Exec→Data rejected
  - Data→Exec rejected
  - Bool→Int rejected
  - Actor→ObjectRef accepted
  - Data input pin: second connection replaces first
  - Exec output pin: connects to two inputs successfully
- [ ] Acceptance: all connection-rule tests pass; `CanConnect` agrees with `Connect`

---

### 2.4 — imgui-node-editor canvas `P0`

> ROADMAP §28 (Canvas UX)

- [ ] Update `src/ui/GraphEditorPanel.h` / `GraphEditorPanel.cpp` (replace existing stub):
  - Hold an `ax::NodeEditor::EditorContext*` created/destroyed with the panel
  - `Render(ScriptGraph& graph)` — called each frame; renders all nodes and connections
  - Node rendering:
    - Header bar: category colour + `display_name` + category tag (e.g. `[EVENT]`)
    - Left-side input pins with type-coloured circle (from `PinColor()`)
    - Right-side output pins
    - Exec pins rendered as white triangles; Data pins as filled/empty circles
  - Connection rendering: wires use `PinColor()` of the source pin
  - `BeginCreate` / `EndCreate` handling — validates connection via `ScriptGraph::CanConnect()`; creates if valid; shows red X cursor if invalid
  - `BeginDelete` / `EndDelete` handling — removes nodes and connections from graph
- [ ] Canvas interactions (provided by imgui-node-editor):
  - Middle-mouse drag / Alt+drag → pan
  - Scroll wheel → zoom (clamp: 10% – 400%)
  - `F` → frame all nodes (`NavigateToContent`)
  - `Shift+F` → frame selection (`NavigateToSelection`)
- [ ] Rubber-band multi-select (built into imgui-node-editor)
- [ ] Node placement: `ScriptGraph::AddNode()` called with position from canvas right-click (wired in 2.11)
- [ ] Acceptance: nodes render with correct colours; wires connect and disconnect; zoom/pan works; frame-all frames all nodes

**Files to modify:**
```
src/ui/GraphEditorPanel.h
src/ui/GraphEditorPanel.cpp
```

---

### 2.5 — Tool Palette panel `P0`

> ROADMAP §24

- [ ] Update `src/ui/ToolPalettePanel.h` / `ToolPalettePanel.cpp` (replace existing stub):
  - Queries `NodeRegistry::Get()` for all nodes
  - Collapsible category sections using `ImGui::TreeNodeEx` with category header colour
  - Category expand/collapse state persisted in `Settings` under `ui.palette_state` (map of category→bool)
  - Each node entry shows `display_name`; hovering shows tooltip from `NodeDefinition::tooltip`
- [ ] Live search filter:
  - `ImGui::InputText` at the top; filters on every keypress (no Enter required)
  - Matches on `display_name`, `category` name, `source_script`
  - When search is active: flat list (no category headers); matched substring bold-highlighted
  - Zero results: show `"No nodes match '<query>'"` message
  - Search cleared when canvas tab switches
- [ ] Drag-and-drop from palette to canvas:
  - `ImGui::BeginDragDropSource` on each node entry
  - Payload: `type_id` string
  - Canvas accepts `ImGui::BeginDragDropTarget`; calls `ScriptGraph::AddNode()` at drop position
- [ ] Acceptance: all 61 built-in nodes visible; collapse/expand works; search filters correctly; drag-to-canvas places node

**Files to modify:**
```
src/ui/ToolPalettePanel.h
src/ui/ToolPalettePanel.cpp
```

---

### 2.6 — JSON graph serialization `P0`

> ROADMAP §17 (JSON Mirror Format), §81

- [ ] Create `src/graph/GraphSerializer.h` / `GraphSerializer.cpp`:
  - `GraphSerializer::Save(const ScriptGraph&) → nlohmann::json`
  - `GraphSerializer::Load(const nlohmann::json&) → ScriptGraph`
  - JSON schema per ROADMAP §17:
    ```json
    {
      "script_name": "MyQuestScript",
      "extends": "Quest",
      "next_node_id": 5,
      "nodes": [
        { "id": 1, "type_id": "builtin.OnInit", "x": 100.0, "y": 200.0,
          "pins": [{ "id": 65536, "value": "" }] }
      ],
      "connections": [
        { "id": 1, "from": 65537, "to": 131072 }
      ]
    }
    ```
  - Missing/unknown `type_id` on load → log warning, skip node (forward-compatibility)
  - Pin `value` field stores default value overrides (e.g. Literal Int node's constant)
- [ ] Integrate into `Project::Save()` / `Project::Load()`:
  - `Project` stores `std::vector<ScriptGraph> scripts`
  - Each script serialized under `"scripts"` array in `.skyscribe` JSON
- [ ] **Unit tests** (`tests/test_graph_serializer.cpp`):
  - Round-trip: build graph with 3 nodes + 2 connections → `Save()` → `Load()` → identical graph
  - `Load()` with unknown `type_id` skips node without crashing
  - `next_node_id` is restored correctly so subsequent `AddNode()` never reuses an ID
  - Empty graph serializes and deserializes cleanly
- [ ] Acceptance: 4+ tests pass; save → close → reload = identical graph state

**Files to create:**
```
src/graph/GraphSerializer.h
src/graph/GraphSerializer.cpp
tests/test_graph_serializer.cpp
```

---

### 2.7 — Undo / Redo stack `P1`

> ROADMAP §23, §28

- [ ] Create `src/graph/UndoStack.h` / `UndoStack.cpp`:
  - `struct ICommand { virtual void Execute(ScriptGraph&) = 0; virtual void Undo(ScriptGraph&) = 0; virtual std::string Description() const = 0; }`
  - Concrete commands (all value-copy — no raw pointers into live graph):
    - `AddNodeCmd { ScriptNode snapshot; }`
    - `RemoveNodeCmd { ScriptNode snapshot; std::vector<Connection> removed_connections; }`
    - `MoveNodeCmd { uint64_t node_id; float old_x, old_y, new_x, new_y; }`
    - `ConnectCmd { Connection conn; }`
    - `DisconnectCmd { Connection conn; }`
    - `SetPinValueCmd { uint64_t pin_id; std::string old_value, new_value; }`
    - `RenameScriptCmd { std::string old_name, new_name; }`
    - `SetExtendsCmd { std::string old_extends, new_extends; }`
    - `MacroCmd { std::vector<std::unique_ptr<ICommand>> cmds; std::string description; }` — wraps multi-node operations (alignment, group move, paste)
  - `UndoStack::Push(std::unique_ptr<ICommand>)` — clears redo list; caps history at 100 steps
  - `UndoStack::Undo(ScriptGraph&)` / `Undo(ScriptGraph&)`
  - `UndoStack::CanUndo() / CanRedo() → bool`
  - `UndoStack::UndoDescription() / RedoDescription() → std::string`
  - `UndoStack::Clear()` — called on project close
- [ ] Wire `Ctrl+Z` / `Ctrl+Y` / `Ctrl+Shift+Z` in `MainWindow::Render()` hotkey block
- [ ] All graph mutations in `GraphEditorPanel` (create/delete via imgui-node-editor callbacks) push commands through `UndoStack`
- [ ] **Unit tests** (`tests/test_undo_stack.cpp`):
  - Add node → undo → graph empty; redo → node back
  - Connect → undo → connection gone; redo → reconnected
  - 101 pushes → history capped at 100; oldest entry gone
  - `MacroCmd`: add 3 nodes as macro → single undo removes all 3
- [ ] Acceptance: 4+ tests pass; Ctrl+Z/Y correct for all command types

**Files to create:**
```
src/graph/UndoStack.h
src/graph/UndoStack.cpp
tests/test_undo_stack.cpp
```

---

### 2.8 — Multi-script project support `P0`

> ROADMAP §10

- [ ] Update `src/project/Project.h` / `Project.cpp`:
  - `Project` holds `std::vector<ScriptGraph> scripts`
  - `AddScript(name, extends) → ScriptGraph&`
  - `RemoveScript(index)`
  - `RenameScript(index, new_name)`
  - Dirty flag set on any of the above
- [ ] Create `src/ui/ProjectPanel.h` / `ProjectPanel.cpp`:
  - Dockable ImGui panel, label `"Project"`
  - Lists all scripts in current project with name and `extends` type
  - `[+ New Script]` button → opens inline name/extends dialog → calls `Project::AddScript()`
  - Rename inline (double-click label → `InputText`)
  - Delete button (🗑) with confirmation tooltip (`"Hold Shift to delete"`)
  - Clicking a script activates its canvas tab (wired to `GraphEditorPanel`)
  - Active script highlighted
- [ ] Canvas tab bar in `GraphEditorPanel`:
  - One `ImGui::BeginTabBar` tab per script
  - Clicking a tab switches the active `ScriptGraph*`
  - Tab close button triggers "unsaved changes?" check if dirty
  - New tabs added when `Project::AddScript()` is called
- [ ] **Script Identity Bar** at top of each canvas (per ROADMAP §12):
  - `ScriptName` text input — alphanumeric + `_` only; spaces auto-converted to `_`; max 128 chars
  - `Extends` combo/text field — dropdown populated from built-in type hierarchy (Form, ObjectReference, Actor, Quest, Weapon, Armor, Spell, MagicEffect, ActiveMagicEffect, Activator, Container, Door); free-text for custom types
  - Changes push `RenameScriptCmd` / `SetExtendsCmd` to `UndoStack`
- [ ] Acceptance: create 3 scripts; switch between them; rename one; delete one; all persists through save/reload

**Files to create / modify:**
```
src/project/Project.h/.cpp   ← add scripts vector + methods
src/ui/ProjectPanel.h
src/ui/ProjectPanel.cpp
src/ui/GraphEditorPanel.h/.cpp  ← add tab bar + identity bar
```

---

### 2.9 — Palette category persistence `P0`

> ROADMAP §24 (Palette Implementation Notes)

- [ ] Add `std::map<std::string, bool> palette_category_expanded` to `Settings`
- [ ] Serialize under `"ui"."palette_state"` in `config.json`
- [ ] `ToolPalettePanel` reads/writes this map on category toggle
- [ ] Acceptance: collapse Events category, restart app — Events is still collapsed

---

### 2.10 — Canvas right-click node picker `P1`

> ROADMAP §24 (Canvas Right-Click Node Picker)

- [ ] Right-click on blank canvas space opens a floating `ImGui::BeginPopup("##node_picker")`
- [ ] Auto-focused search `InputText`; pre-populated with all nodes
- [ ] Node list filtered live; category breadcrumb shown next to each result
- [ ] Selecting a node closes popup, calls `ScriptGraph::AddNode()` at right-click canvas position, pushes `AddNodeCmd`
- [ ] ESC closes without placing
- [ ] Acceptance: right-click → type "notify" → `Debug.Notification` appears → select → node placed at cursor

---

### 2.11 — Pin-drag filtered node picker `P1`

> ROADMAP §24 (Pin-Drag Contextual Filter)

- [ ] When user drags a wire from an output pin and releases on blank canvas space, open node picker
- [ ] Picker pre-filtered to nodes that have at least one compatible input pin for the dragged type
- [ ] Selecting a node places it and auto-connects the compatible pin
- [ ] Acceptance: drag from `Actor` output pin → picker shows only nodes with `Actor` or `ObjectRef` input pins → select → node placed and connected

---

### 2.12 — Copy / Cut / Paste / Duplicate `P1`

> ROADMAP §27

- [ ] Create `src/graph/ClipboardPayload.h`:
  - `struct ClipboardPayload { std::vector<ScriptNode> nodes; std::vector<Connection> internal_connections; }`
  - Only connections where **both** endpoints are in the copied selection are included
- [ ] `GraphEditorPanel`:
  - `Ctrl+C` — copy selected nodes + internal connections to `ClipboardPayload`
  - `Ctrl+X` — same as copy + `RemoveNodeCmd` (via `MacroCmd`) for each selected node
  - `Ctrl+V` — assign new IDs to all pasted nodes/pins; offset positions `+40, +40`; push `MacroCmd(AddNodeCmd × N + ConnectCmd × M)` to undo stack
  - `Ctrl+D` — duplicate without clipboard write; immediate paste with offset
  - Repeated `Ctrl+V` adds additional `+40, +40` offset per paste; offset resets on new copy
- [ ] Acceptance: copy 2 connected nodes → paste → 2 new nodes with same connection; undo removes both

---

### 2.13 — Node context menu `P1`

> ROADMAP §28 (Node Right-Click)

- [ ] Right-clicking a **node** opens node context menu (not the canvas picker):
  - Cut / Copy / Duplicate / Delete
  - **Align** submenu (visible only when ≥ 2 nodes selected):
    - Left edges, Right edges, Top edges, Bottom edges
    - Centre horizontal, Centre vertical
    - Distribute H, Distribute V
- [ ] All alignment operations produce a `MacroCmd` of `MoveNodeCmd`s — fully undoable
- [ ] Acceptance: select 3 nodes → right-click → Align → Left edges → all left edges match leftmost; Ctrl+Z restores original positions

---

### 2.14 — Project Panel wired into MainWindow layout `P0`

- [ ] Add `ProjectPanel project_panel_` member to `MainWindow`
- [ ] Dock `"Project"` panel into the default layout (left column, below Tool Palette, or replace it)
  - Update `BuildDefaultLayout()` to split `dock_palette` into palette (top ~50%) + project (bottom ~50%)
- [ ] Add `project_panel_.Render()` call in `MainWindow::Render()`
- [ ] Acceptance: Project panel visible in default layout; shows current project's script list

---

## File Layout After Phase 2

```
src/
├── graph/
│   ├── PinType.h           ✅ Phase 1
│   ├── PinColorMap.h       ✅ Phase 1
│   ├── NodeDefinition.h    ← 2.1
│   ├── ScriptGraph.h/.cpp  ← 2.1
│   ├── NodeRegistry.h/.cpp ← 2.2
│   ├── BuiltinNodes.h/.cpp ← 2.2
│   ├── GraphSerializer.h/.cpp ← 2.6
│   └── UndoStack.h/.cpp    ← 2.7
├── ui/
│   ├── GraphEditorPanel.h/.cpp  ← 2.4 + 2.8 (canvas + tabs + identity bar)
│   ├── ToolPalettePanel.h/.cpp  ← 2.5 (palette + search + DnD)
│   └── ProjectPanel.h/.cpp      ← 2.8
tests/
├── test_graph_model.cpp     ← 2.1 + 2.3
├── test_node_registry.cpp   ← 2.2
├── test_graph_serializer.cpp ← 2.6
└── test_undo_stack.cpp      ← 2.7
```

---

## Definition of Done

- [ ] `cmake --preset release` builds with zero errors and zero warnings
- [ ] All new unit tests pass (`ctest --preset release`): graph model (≥5), node registry (≥4), serializer (≥4), undo stack (≥4)
- [ ] All 61 built-in nodes (§8 ROADMAP) are registered and visible in the Tool Palette
- [ ] Nodes can be placed on canvas via palette drag-drop and right-click picker
- [ ] Typed pin connections enforce `IsCompatible()` — invalid wires rejected at UI level
- [ ] Graph save → close → reload produces an identical graph state
- [ ] Undo/redo works for: add node, delete node, connect, disconnect, move node, paste
- [ ] Multi-script project: create 3 scripts, switch between canvases, rename, delete — all persists
- [ ] Script Identity Bar: `ScriptName` and `Extends` editable per canvas; changes are undoable
- [ ] Canvas zoom/pan, frame-all (`F`), rubber-band select all function correctly
