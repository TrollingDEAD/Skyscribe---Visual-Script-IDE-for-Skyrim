#include "ui/GraphEditorPanel.h"
#include "graph/NodeRegistry.h"
#include "graph/PinColorMap.h"
#include "graph/UndoStack.h"
#include "project/Project.h"
#include "app/Logger.h"

#include "graph/PinType.h"

#include <imgui_node_editor.h>
#include <imgui_internal.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace ne = ax::NodeEditor;

namespace {

// ── Category helpers ──────────────────────────────────────────────────────────

static const char* CategoryName(graph::NodeCategory cat) {
    switch (cat) {
    case graph::NodeCategory::Event:       return "Events";
    case graph::NodeCategory::ControlFlow: return "Control Flow";
    case graph::NodeCategory::Variable:    return "Variables";
    case graph::NodeCategory::Math:        return "Math & Logic";
    case graph::NodeCategory::Debug:       return "Debug";
    case graph::NodeCategory::Actor:       return "Actor";
    case graph::NodeCategory::Quest:       return "Quest / Story";
    case graph::NodeCategory::Utility:     return "Utility / Timer";
    case graph::NodeCategory::Custom:      return "Custom";
    default:                               return "Other";
    }
}

static ImVec4 CategoryColor(graph::NodeCategory cat) {
    switch (cat) {
    case graph::NodeCategory::Event:       return ImVec4(0.80f, 0.20f, 0.20f, 1.0f);
    case graph::NodeCategory::ControlFlow: return ImVec4(0.30f, 0.30f, 0.80f, 1.0f);
    case graph::NodeCategory::Variable:    return ImVec4(0.20f, 0.60f, 0.20f, 1.0f);
    case graph::NodeCategory::Math:        return ImVec4(0.40f, 0.40f, 0.90f, 1.0f);
    case graph::NodeCategory::Debug:       return ImVec4(0.90f, 0.60f, 0.10f, 1.0f);
    case graph::NodeCategory::Actor:       return ImVec4(0.60f, 0.30f, 0.85f, 1.0f);
    case graph::NodeCategory::Quest:       return ImVec4(0.90f, 0.80f, 0.20f, 1.0f);
    case graph::NodeCategory::Utility:     return ImVec4(0.30f, 0.70f, 0.70f, 1.0f);
    default:                               return ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    }
}

static const char* CategoryTag(graph::NodeCategory cat) {
    switch (cat) {
    case graph::NodeCategory::Event:       return "EVENT";
    case graph::NodeCategory::ControlFlow: return "FLOW";
    case graph::NodeCategory::Variable:    return "VAR";
    case graph::NodeCategory::Math:        return "MATH";
    case graph::NodeCategory::Debug:       return "DEBUG";
    case graph::NodeCategory::Actor:       return "ACTOR";
    case graph::NodeCategory::Quest:       return "QUEST";
    case graph::NodeCategory::Utility:     return "UTIL";
    default:                               return "???";
    }
}

} // anonymous namespace

namespace ui {

// ── Lifecycle ─────────────────────────────────────────────────────────────────

GraphEditorPanel::~GraphEditorPanel() {
    if (ctx_) ne::DestroyEditor(ctx_);
    for (auto& kv : func_ctxs_) if (kv.second) ne::DestroyEditor(kv.second);
}

// ── Render ────────────────────────────────────────────────────────────────────

void GraphEditorPanel::Render() {
    ImGui::Begin("Graph Editor");

    auto& proj = project::Project::Get();
    if (!proj.IsOpen() || proj.Scripts().empty()) {
        ImGui::TextDisabled("Open or create a project to edit scripts.");
        ImGui::End();
        return;
    }

    // Lazy-init editor context (ImGui must be running first)
    if (!ctx_) {
        ne::Config cfg;
        cfg.SettingsFile = nullptr; // we persist positions ourselves
        ctx_ = ne::CreateEditor(&cfg);
    }

    auto& scripts = proj.Scripts();
    int   active  = proj.ActiveScriptIndex();

    // ── Script tab bar ────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##script_tabs")) {
        for (int i = 0; i < static_cast<int>(scripts.size()); ++i) {
            const bool sel = (i == active);
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (sel) flags |= ImGuiTabItemFlags_SetSelected;

            if (ImGui::BeginTabItem(scripts[i].script_name.c_str(), nullptr, flags)) {
                if (i != active) proj.SetActiveScript(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    if (active < 0 || active >= static_cast<int>(scripts.size())) {
        ImGui::End();
        return;
    }

    auto& g = scripts[static_cast<size_t>(active)];

    // ── Script identity bar ───────────────────────────────────────────────────
    if (last_script_name_ != g.script_name) {
        // Active graph switched → clear position cache & undo
        positioned_nodes_.clear();
        undo_.Clear();
        last_script_name_ = g.script_name;
        std::strncpy(name_buf_,    g.script_name.c_str(), sizeof(name_buf_) - 1);
        std::strncpy(extends_buf_, g.extends.c_str(),     sizeof(extends_buf_) - 1);
        name_buf_[sizeof(name_buf_) - 1]       = '\0';
        extends_buf_[sizeof(extends_buf_) - 1] = '\0';

        // Clean up function editor contexts for old script
        for (auto& kv : func_ctxs_) if (kv.second) ne::DestroyEditor(kv.second);
        func_ctxs_.clear();
        func_positioned_nodes_.clear();
        active_func_tab_ = -1;
        func_tab_script_ = g.script_name;
    }

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("Script Name", name_buf_, sizeof(name_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        proj.RenameScript(static_cast<size_t>(active), name_buf_);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputText("Extends", extends_buf_, sizeof(extends_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        g.extends = extends_buf_;
        proj.MarkDirty();
    }

    ImGui::Separator();

    // ── Function tabs (task 3.9) ──────────────────────────────────────────────
    // Clamp active_func_tab_ in case a function was removed externally
    if (active_func_tab_ >= static_cast<int>(g.functions.size()))
        active_func_tab_ = -1;

    if (ImGui::BeginTabBar("##func_tabs")) {
        // "Event Graph" tab
        ImGuiTabItemFlags ev_flags = (active_func_tab_ == -1)
            ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Event Graph", nullptr, ev_flags)) {
            active_func_tab_ = -1;
            ImGui::EndTabItem();
        }
        // Per-function tabs
        for (int i = 0; i < static_cast<int>(g.functions.size()); ++i) {
            bool open = true;
            ImGuiTabItemFlags fl = (active_func_tab_ == i)
                ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(g.functions[i].name.c_str(), &open, fl)) {
                if (active_func_tab_ != i) active_func_tab_ = i;
                ImGui::EndTabItem();
            }
            if (!open) {
                // User clicked [x] — delete this function
                graph::FunctionDefinition copy = g.functions[i];
                auto cmd = std::make_unique<graph::DeleteFunctionCmd>(copy);
                cmd->Execute(g);
                undo_.Push(std::move(cmd));
                proj.MarkDirty();
                if (active_func_tab_ >= static_cast<int>(g.functions.size()))
                    active_func_tab_ = -1;
            }
        }
        // "+" button to create a new function
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            func_add_dialog_open_ = true;
        ImGui::EndTabBar();
    }

    // Add Function modal
    if (func_add_dialog_open_) {
        ImGui::OpenPopup("AddFunction##modal");
        func_add_dialog_open_ = false;
    }
    if (ImGui::BeginPopupModal("AddFunction##modal", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name##funcname", func_name_buf_, sizeof(func_name_buf_));
        if (ImGui::Button("Add")) {
            if (func_name_buf_[0] != '\0') {
                auto& f = g.AddFunction(func_name_buf_);
                // Push undo command with a snapshot of the newly added function
                graph::FunctionDefinition snapshot = f;
                undo_.Push(std::make_unique<graph::AddFunctionCmd>(snapshot));
                func_name_buf_[0] = '\0';
                proj.MarkDirty();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            func_name_buf_[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Canvas ────────────────────────────────────────────────────────────────
    if (active_func_tab_ == -1 || active_func_tab_ >= static_cast<int>(g.functions.size())) {
        // Event graph
        RenderCanvas(g);
    } else {
        auto& func = g.functions[static_cast<size_t>(active_func_tab_)];
        // Get or create per-function editor context
        if (func_ctxs_.find(func.name) == func_ctxs_.end()) {
            ne::Config cfg; cfg.SettingsFile = nullptr;
            func_ctxs_[func.name] = ne::CreateEditor(&cfg);
        }
        // Swap in function editor context + positioned set
        auto* saved_ctx = ctx_;
        ctx_ = func_ctxs_[func.name];
        auto saved_pos = std::move(positioned_nodes_);
        positioned_nodes_ = std::move(func_positioned_nodes_);

        RenderCanvas(*func.body_graph);

        func_positioned_nodes_ = std::move(positioned_nodes_);
        positioned_nodes_      = std::move(saved_pos);
        ctx_                   = saved_ctx;
    }

    ImGui::End();
}

// ── Canvas rendering ──────────────────────────────────────────────────────────

void GraphEditorPanel::RenderCanvas(graph::ScriptGraph& g) {
    ne::SetCurrentEditor(ctx_);
    ne::Begin("##canvas", ImGui::GetContentRegionAvail());

    // Restore positions for nodes that have never been positioned in this session
    for (auto& node : g.nodes) {
        if (positioned_nodes_.find(node.id) == positioned_nodes_.end()) {
            ne::SetNodePosition(ne::NodeId(node.id), ImVec2(node.pos_x, node.pos_y));
            positioned_nodes_.insert(node.id);
        }
    }

    // Draw all nodes
    for (auto& node : g.nodes)
        DrawNode(g, node);

    // Draw all connections (wire colour = source pin type)
    for (const auto& conn : g.connections) {
        const graph::Pin* from_pin = g.FindPin(conn.from_pin_id);
        ImVec4 wire_color = from_pin
            ? graph::PinColor(from_pin->type)
            : ImVec4(1, 1, 1, 1);
        ne::Link(ne::LinkId(conn.id),
                 ne::PinId(conn.from_pin_id),
                 ne::PinId(conn.to_pin_id),
                 wire_color, 2.0f);
    }

    HandleCreate(g);
    HandleDelete(g);
    HandleKeyboardShortcuts(g);

    // ── Right-click on blank canvas → node picker ─────────────────────────────
    if (ne::ShowBackgroundContextMenu()) {
        picker_canvas_pos_  = ne::ScreenToCanvas(ImGui::GetMousePos());
        picker_from_pin_id_ = 0;
        picker_open_        = true;
        picker_search_[0]   = '\0';
    }

    // ── Right-click on node → context menu ───────────────────────────────────
    ne::NodeId hovered_node;
    if (ne::ShowNodeContextMenu(&hovered_node)) {
        ctx_node_id_ = hovered_node.Get();
        ImGui::OpenPopup("##node_ctx");
    }

    ne::End();
    ne::SetCurrentEditor(nullptr);

    // ── Popups outside ne::Begin/End ──────────────────────────────────────────
    RenderNodePicker(g, picker_canvas_pos_, picker_from_pin_id_);
    RenderNodeContextMenu(g);

    // Check for drag-drop from ToolPalette (must be outside ne::Begin/End)
    AcceptPaletteDrop(g);

    // ── F / Shift+F shortcuts outside the editor ──────────────────────────────
    // These are also handled inside HandleKeyboardShortcuts via ne::IsActive.
}

// ── Node drawing ──────────────────────────────────────────────────────────────

void GraphEditorPanel::DrawNode(graph::ScriptGraph& g, graph::ScriptNode& node) {
    const graph::NodeDefinition* def =
        graph::NodeRegistry::Get().Find(node.type_id);

    const char*  tag       = def ? CategoryTag(def->category)    : "???";
    ImVec4       cat_color = def ? CategoryColor(def->category)  : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    const char*  name      = def ? def->display_name.c_str()     : node.type_id.c_str();

    ne::PushStyleColor(ne::StyleColor_NodeBorder, cat_color);
    ne::BeginNode(ne::NodeId(node.id));

    // Header
    ImVec2 header_start = ImGui::GetCursorScreenPos();
    ImGui::TextColored(cat_color, "[%s]", tag);
    ImGui::SameLine();
    ImGui::TextUnformatted(name);
    ImVec2 header_end = ImGui::GetCursorScreenPos();

    // Draw colored header background via node background draw list
    {
        ImVec2 np   = ne::GetNodePosition(ne::NodeId(node.id));
        ImVec2 ns   = ne::GetNodeSize(ne::NodeId(node.id));
        float  pad  = ne::GetStyle().NodePadding.x;
        float  hh   = header_end.y - header_start.y + 4.0f; // header height
        auto*  dl   = ne::GetNodeBackgroundDrawList(ne::NodeId(node.id));
        ImU32  col  = ImGui::ColorConvertFloat4ToU32(
                          ImVec4(cat_color.x * 0.4f, cat_color.y * 0.4f,
                                 cat_color.z * 0.4f, 0.6f));
        dl->AddRectFilled(
            ImVec2(np.x - pad, np.y - pad),
            ImVec2(np.x + ns.x + pad, np.y - pad + hh),
            col,
            ne::GetStyle().NodeRounding,
            ImDrawFlags_RoundCornersTop);
    }

    ImGui::Separator();

    // Input pins
    for (auto& pin : node.pins) {
        if (pin.kind != graph::PinKind::Input) continue;
        ne::BeginPin(ne::PinId(pin.id), ne::PinKind::Input);
        DrawPinIcon(pin);
        ImGui::SameLine();
        ImGui::TextUnformatted(pin.name.c_str());
        ne::EndPin();
    }

    // Output pins (right-aligned with some spacing)
    for (auto& pin : node.pins) {
        if (pin.kind != graph::PinKind::Output) continue;

        // Indent so output pins appear on the right
        float text_w = ImGui::CalcTextSize(pin.name.c_str()).x + 18.0f;
        float avail  = ImGui::GetContentRegionAvail().x;
        if (avail > text_w)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - text_w));

        ne::BeginPin(ne::PinId(pin.id), ne::PinKind::Output);
        ImGui::TextUnformatted(pin.name.c_str());
        ImGui::SameLine();
        DrawPinIcon(pin);
        ne::EndPin();
    }

    ne::EndNode();
    ne::PopStyleColor(1);
}

// ── Pin icon ──────────────────────────────────────────────────────────────────

void GraphEditorPanel::DrawPinIcon(const graph::Pin& p) {
    constexpr float kRadius = 6.0f;
    constexpr float kSize   = kRadius * 2.0f + 2.0f;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    ImVec2      ctr = ImVec2(pos.x + kRadius + 1.0f, pos.y + kRadius + 1.0f);

    ImU32 color = ImGui::ColorConvertFloat4ToU32(graph::PinColor(p.type));

    if (p.flow == graph::PinFlow::Execution) {
        // White triangle
        if (p.kind == graph::PinKind::Output) {
            dl->AddTriangleFilled(
                ImVec2(ctr.x - kRadius, ctr.y - kRadius),
                ImVec2(ctr.x + kRadius, ctr.y),
                ImVec2(ctr.x - kRadius, ctr.y + kRadius),
                IM_COL32(255, 255, 255, 220));
        } else {
            dl->AddTriangleFilled(
                ImVec2(ctr.x + kRadius, ctr.y - kRadius),
                ImVec2(ctr.x - kRadius, ctr.y),
                ImVec2(ctr.x + kRadius, ctr.y + kRadius),
                IM_COL32(255, 255, 255, 220));
        }
    } else {
        // Filled circle with type colour
        dl->AddCircleFilled(ctr, kRadius, color);
        dl->AddCircle(ctr, kRadius, IM_COL32(255, 255, 255, 80), 12, 1.0f);
    }

    ImGui::Dummy(ImVec2(kSize, kSize));
}

// ── Create links ──────────────────────────────────────────────────────────────

void GraphEditorPanel::HandleCreate(graph::ScriptGraph& g) {
    if (!ne::BeginCreate(ImVec4(1, 1, 1, 1), 2.0f)) {
        ne::EndCreate();
        return;
    }

    ne::PinId from_id, to_id;
    if (ne::QueryNewLink(&from_id, &to_id)) {
        uint64_t fid = from_id.Get();
        uint64_t tid = to_id.Get();

        // Auto-flip: user may drag from Input → Output
        if (g.FindPin(fid) && g.FindPin(fid)->kind == graph::PinKind::Input)
            std::swap(fid, tid);

        if (g.CanConnect(fid, tid)) {
            if (ne::AcceptNewItem(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), 3.0f)) {
                // Capture any displaced connection before connecting
                std::optional<graph::Connection> replaced;
                const graph::Pin* to_pin = g.FindPin(tid);
                if (to_pin && to_pin->flow == graph::PinFlow::Data &&
                    to_pin->kind == graph::PinKind::Input) {
                    auto existing = g.ConnectionsForPin(tid);
                    if (!existing.empty()) replaced = *existing[0];
                }
                uint64_t cid = g.Connect(fid, tid);
                if (cid != 0) {
                    graph::Connection recorded = g.connections.back();
                    undo_.Push(std::make_unique<graph::ConnectCmd>(recorded, replaced));
                    project::Project::Get().MarkDirty();
                }
            }
        } else {
            ne::RejectNewItem(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), 2.0f);
        }
    }

    // ── Pin drag released on blank canvas → filtered node picker ─────────────
    ne::PinId dragged_pin;
    if (ne::QueryNewNode(&dragged_pin)) {
        ne::AcceptNewItem();
        uint64_t pid = dragged_pin.Get();
        // Normalise: we want the Output pin as the "from"
        const graph::Pin* p = g.FindPin(pid);
        if (p && p->kind == graph::PinKind::Output) {
            picker_canvas_pos_  = ne::ScreenToCanvas(ImGui::GetMousePos());
            picker_from_pin_id_ = pid;
            picker_open_        = true;
            picker_search_[0]   = '\0';
        }
    }

    ne::EndCreate();
}

// ── Delete nodes/links ────────────────────────────────────────────────────────

void GraphEditorPanel::HandleDelete(graph::ScriptGraph& g) {
    if (!ne::BeginDelete()) {
        ne::EndDelete();
        return;
    }

    // Delete links
    ne::LinkId link_id;
    while (ne::QueryDeletedLink(&link_id)) {
        if (ne::AcceptDeletedItem()) {
            uint64_t cid = link_id.Get();
            auto it = std::find_if(g.connections.begin(), g.connections.end(),
                [cid](const graph::Connection& c) { return c.id == cid; });
            if (it != g.connections.end()) {
                undo_.Push(std::make_unique<graph::DisconnectCmd>(*it));
                g.Disconnect(cid);
                project::Project::Get().MarkDirty();
            }
        }
    }

    // Delete nodes
    ne::NodeId node_id;
    while (ne::QueryDeletedNode(&node_id)) {
        if (ne::AcceptDeletedItem()) {
            uint64_t nid = node_id.Get();
            graph::ScriptNode* n = g.FindNode(nid);
            if (n) {
                // Capture connections before removal
                std::vector<graph::Connection> removed_conns;
                for (const auto& p : n->pins)
                    for (const auto* c : g.ConnectionsForPin(p.id))
                        removed_conns.push_back(*c);

                graph::ScriptNode snap = *n;
                g.RemoveNode(nid);
                positioned_nodes_.erase(nid);
                undo_.Push(std::make_unique<graph::RemoveNodeCmd>(
                    std::move(snap), std::move(removed_conns)));
                project::Project::Get().MarkDirty();
            }
        }
    }

    ne::EndDelete();
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────

void GraphEditorPanel::HandleKeyboardShortcuts(graph::ScriptGraph& g) {
    if (!ne::IsActive()) return;

    auto& io = ImGui::GetIO();

    // F = navigate to all content
    if (ImGui::IsKeyPressed(ImGuiKey_F) && !io.KeyCtrl && !io.KeyShift)
        ne::NavigateToContent();

    // Shift+F = navigate to selection
    if (ImGui::IsKeyPressed(ImGuiKey_F) && io.KeyShift)
        ne::NavigateToSelection(false);

    // Ctrl+Z = undo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift) {
        if (undo_.CanUndo()) {
            undo_.Undo(g);
            positioned_nodes_.clear(); // positions may have changed
            project::Project::Get().MarkDirty();
        }
    }

    // Ctrl+Y / Ctrl+Shift+Z = redo
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) ||
                       (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))) {
        if (undo_.CanRedo()) {
            undo_.Redo(g);
            positioned_nodes_.clear();
            project::Project::Get().MarkDirty();
        }
    }

    // Ctrl+C = copy
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !io.KeyShift)
        CopySelected(g);

    // Ctrl+X = cut
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X))
        CutSelected(g);

    // Ctrl+V = paste
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
        PasteClipboard(g);

    // Ctrl+D = duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
        DuplicateSelected(g);
}

// ── Palette drag-drop acceptance ──────────────────────────────────────────────

void GraphEditorPanel::AcceptPaletteDrop(graph::ScriptGraph& g) {
    if (!ImGui::BeginDragDropTarget()) return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_TYPE")) {
        std::string type_id(static_cast<const char*>(payload->Data),
                            static_cast<size_t>(payload->DataSize) - 1);

        const graph::NodeDefinition* def =
            graph::NodeRegistry::Get().Find(type_id);
        if (def) {
            ne::SetCurrentEditor(ctx_);
            ImVec2 canvas_pos = ne::ScreenToCanvas(ImGui::GetMousePos());
            ne::SetCurrentEditor(nullptr);

            uint64_t id = g.AddNode(*def, canvas_pos.x, canvas_pos.y);
            positioned_nodes_.insert(id); // position already set via AddNode
            // Set the imgui-node-editor position on the next frame via ctx
            ne::SetCurrentEditor(ctx_);
            ne::SetNodePosition(ne::NodeId(id), canvas_pos);
            ne::SetCurrentEditor(nullptr);

            undo_.Push(std::make_unique<graph::AddNodeCmd>(*g.FindNode(id)));
            project::Project::Get().MarkDirty();
        }
    }
    ImGui::EndDragDropTarget();
}

// ── Sync positions before save ────────────────────────────────────────────────

void GraphEditorPanel::SyncNodePositions() {
    auto& proj = project::Project::Get();
    if (!proj.IsOpen() || !ctx_) return;

    ne::SetCurrentEditor(ctx_);
    for (auto& script : proj.Scripts()) {
        for (auto& node : script.nodes) {
            ImVec2 pos = ne::GetNodePosition(ne::NodeId(node.id));
            node.pos_x = pos.x;
            node.pos_y = pos.y;
        }
    }
    ne::SetCurrentEditor(nullptr);
}

// ── Node Picker (right-click / pin-drag) ──────────────────────────────────────

void GraphEditorPanel::RenderNodePicker(graph::ScriptGraph& g, ImVec2 canvas_pos,
                                        uint64_t from_pin_id) {
    if (!picker_open_) return;
    ImGui::OpenPopup("##node_picker");
    picker_open_ = false;

    ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup("##node_picker")) return;

    // Auto-focus search on open
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##picker_search", picker_search_, sizeof(picker_search_));

    ImGui::Separator();

    const auto& all_nodes = graph::NodeRegistry::Get().AllNodes();

    // Collect matches (optionally filtered by compatible input pins for from_pin)
    const graph::Pin* from_pin = from_pin_id ? g.FindPin(from_pin_id) : nullptr;

    for (const auto& def : all_nodes) {
        // Filter by search text
        if (picker_search_[0] != '\0') {
            bool match = false;
            auto ci = [](const std::string& s, const char* q) {
                std::string h = s, n(q);
                std::transform(h.begin(), h.end(), h.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                std::transform(n.begin(), n.end(), n.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                return h.find(n) != std::string::npos;
            };
            if (ci(def.display_name, picker_search_) ||
                ci(CategoryName(def.category), picker_search_) ||
                ci(def.source_script, picker_search_))
                match = true;
            if (!match) continue;
        }

        // Pin-drag filter: only show nodes that have at least one compatible input pin
        if (from_pin) {
            bool has_compat = false;
            for (const auto& pd : def.pins) {
                if (pd.kind == graph::PinKind::Input &&
                    pd.flow == from_pin->flow &&
                    (pd.flow == graph::PinFlow::Execution ||
                     graph::IsCompatible(from_pin->type, pd.type))) {
                    has_compat = true;
                    break;
                }
            }
            if (!has_compat) continue;
        }

        // Category breadcrumb
        ImGui::TextDisabled("[%s]", CategoryName(def.category));
        ImGui::SameLine();

        if (ImGui::Selectable(def.display_name.c_str())) {
            uint64_t id = g.AddNode(def, canvas_pos.x, canvas_pos.y);
            ne::SetCurrentEditor(ctx_);
            ne::SetNodePosition(ne::NodeId(id), canvas_pos);
            ne::SetCurrentEditor(nullptr);
            positioned_nodes_.insert(id);
            undo_.Push(std::make_unique<graph::AddNodeCmd>(*g.FindNode(id)));

            // Auto-connect if triggered from pin drag
            if (from_pin) {
                graph::ScriptNode* new_node = g.FindNode(id);
                if (new_node) {
                    for (auto& p : new_node->pins) {
                        if (p.kind == graph::PinKind::Input &&
                            p.flow == from_pin->flow &&
                            (p.flow == graph::PinFlow::Execution ||
                             graph::IsCompatible(from_pin->type, p.type))) {
                            uint64_t cid = g.Connect(from_pin_id, p.id);
                            if (cid != 0)
                                undo_.Push(std::make_unique<graph::ConnectCmd>(
                                    g.connections.back(), std::nullopt));
                            break;
                        }
                    }
                }
            }

            project::Project::Get().MarkDirty();
            ImGui::CloseCurrentPopup();
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

// ── Selection helper (forward-declared free function) ───────────────────────

static std::vector<uint64_t> GetSelectedNodeIds() {
    int count = ne::GetSelectedObjectCount();
    std::vector<ne::NodeId> ids(static_cast<size_t>(count));
    int n = ne::GetSelectedNodes(ids.data(), count);
    std::vector<uint64_t> result;
    result.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) result.push_back(ids[static_cast<size_t>(i)].Get());
    return result;
}

// ── Node context menu ─────────────────────────────────────────────────────────

void GraphEditorPanel::RenderNodeContextMenu(graph::ScriptGraph& g) {
    if (!ImGui::BeginPopup("##node_ctx")) return;

    const bool multi = ne::GetSelectedObjectCount() > 1;

    if (ImGui::MenuItem("Cut"))       { CutSelected(g); ImGui::CloseCurrentPopup(); }
    if (ImGui::MenuItem("Copy"))      { CopySelected(g); ImGui::CloseCurrentPopup(); }
    if (ImGui::MenuItem("Duplicate")) { DuplicateSelected(g); ImGui::CloseCurrentPopup(); }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) {
        // Delete the right-clicked node (and any selected nodes)
        auto to_del = GetSelectedNodeIds();
        if (to_del.empty()) to_del.push_back(ctx_node_id_);
        std::vector<std::unique_ptr<graph::ICommand>> cmds;
        for (uint64_t nid : to_del) {
            graph::ScriptNode* n = g.FindNode(nid);
            if (!n) continue;
            std::vector<graph::Connection> removed_conns;
            for (const auto& p : n->pins)
                for (const auto* c : g.ConnectionsForPin(p.id))
                    removed_conns.push_back(*c);
            graph::ScriptNode snap = *n;
            g.RemoveNode(nid);
            positioned_nodes_.erase(nid);
            cmds.push_back(std::make_unique<graph::RemoveNodeCmd>(std::move(snap),
                                                                   std::move(removed_conns)));
        }
        if (!cmds.empty()) {
            undo_.Push(std::make_unique<graph::MacroCmd>(std::move(cmds), "Delete"));
            project::Project::Get().MarkDirty();
        }
        ImGui::CloseCurrentPopup();
    }

    if (multi) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Align")) {
            if (ImGui::MenuItem("Left edges"))           AlignSelected(g, 0);
            if (ImGui::MenuItem("Right edges"))          AlignSelected(g, 1);
            if (ImGui::MenuItem("Top edges"))            AlignSelected(g, 2);
            if (ImGui::MenuItem("Bottom edges"))         AlignSelected(g, 3);
            if (ImGui::MenuItem("Centre horizontal"))    AlignSelected(g, 4);
            if (ImGui::MenuItem("Centre vertical"))      AlignSelected(g, 5);
            if (ImGui::MenuItem("Distribute horizontal"))AlignSelected(g, 6);
            if (ImGui::MenuItem("Distribute vertical"))  AlignSelected(g, 7);
            ImGui::EndMenu();
        }
    }

    ImGui::EndPopup();
}

// ── Clipboard helpers ─────────────────────────────────────────────────────────

void GraphEditorPanel::CopySelected(graph::ScriptGraph& g) {
    auto sel_ids = GetSelectedNodeIds();
    if (sel_ids.empty()) return;

    std::unordered_set<uint64_t> sel_set(sel_ids.begin(), sel_ids.end());
    clipboard_.nodes.clear();
    clipboard_.internal_connections.clear();

    for (uint64_t nid : sel_ids) {
        if (const graph::ScriptNode* n = g.FindNode(nid)) {
            // Sync current canvas position before capturing
            ne::SetCurrentEditor(ctx_);
            ImVec2 pos = ne::GetNodePosition(ne::NodeId(nid));
            ne::SetCurrentEditor(nullptr);
            graph::ScriptNode snap = *n;
            snap.pos_x = pos.x;
            snap.pos_y = pos.y;
            clipboard_.nodes.push_back(std::move(snap));
        }
    }
    // Only keep connections where both endpoints are in selection
    for (const auto& conn : g.connections) {
        uint64_t fn = graph::PinOwnerNodeId(conn.from_pin_id);
        uint64_t tn = graph::PinOwnerNodeId(conn.to_pin_id);
        if (sel_set.count(fn) && sel_set.count(tn))
            clipboard_.internal_connections.push_back(conn);
    }
    has_clipboard_ = true;
    paste_offset_  = 0;
}

void GraphEditorPanel::CutSelected(graph::ScriptGraph& g) {
    CopySelected(g);

    auto sel_ids = GetSelectedNodeIds();
    if (sel_ids.empty()) return;

    std::vector<std::unique_ptr<graph::ICommand>> cmds;
    for (uint64_t nid : sel_ids) {
        graph::ScriptNode* n = g.FindNode(nid);
        if (!n) continue;
        std::vector<graph::Connection> removed_conns;
        for (const auto& p : n->pins)
            for (const auto* c : g.ConnectionsForPin(p.id))
                removed_conns.push_back(*c);
        graph::ScriptNode snap = *n;
        g.RemoveNode(nid);
        positioned_nodes_.erase(nid);
        cmds.push_back(std::make_unique<graph::RemoveNodeCmd>(std::move(snap),
                                                               std::move(removed_conns)));
    }
    if (!cmds.empty()) {
        undo_.Push(std::make_unique<graph::MacroCmd>(std::move(cmds), "Cut"));
        project::Project::Get().MarkDirty();
    }
}

void GraphEditorPanel::PasteClipboard(graph::ScriptGraph& g) {
    if (!has_clipboard_ || clipboard_.nodes.empty()) return;

    paste_offset_ += 1;
    float off = paste_offset_ * 40.0f;

    // Map old node IDs → new node IDs
    std::unordered_map<uint64_t, uint64_t> id_map;

    std::vector<std::unique_ptr<graph::ICommand>> cmds;

    for (const auto& src_node : clipboard_.nodes) {
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(src_node.type_id);
        if (!def) continue;

        float nx = src_node.pos_x + off;
        float ny = src_node.pos_y + off;
        uint64_t new_id = g.AddNode(*def, nx, ny);
        id_map[src_node.id] = new_id;

        ne::SetCurrentEditor(ctx_);
        ne::SetNodePosition(ne::NodeId(new_id), ImVec2(nx, ny));
        ne::SetCurrentEditor(nullptr);
        positioned_nodes_.insert(new_id);

        // Copy pin values from source
        graph::ScriptNode* new_node = g.FindNode(new_id);
        if (new_node) {
            for (size_t i = 0; i < new_node->pins.size() && i < src_node.pins.size(); ++i)
                new_node->pins[i].value = src_node.pins[i].value;
        }

        cmds.push_back(std::make_unique<graph::AddNodeCmd>(*g.FindNode(new_id)));
    }

    // Re-connect internal connections with remapped pin IDs
    for (const auto& conn : clipboard_.internal_connections) {
        // Compute new pin IDs: pin_id = (node_id << 16) | pin_index
        uint64_t old_from_node = graph::PinOwnerNodeId(conn.from_pin_id);
        uint64_t old_to_node   = graph::PinOwnerNodeId(conn.to_pin_id);
        uint64_t from_pin_idx  = conn.from_pin_id & 0xFFFF;
        uint64_t to_pin_idx    = conn.to_pin_id   & 0xFFFF;

        auto fi = id_map.find(old_from_node);
        auto ti = id_map.find(old_to_node);
        if (fi == id_map.end() || ti == id_map.end()) continue;

        uint64_t new_from_pin = graph::MakePinId(fi->second, static_cast<uint32_t>(from_pin_idx));
        uint64_t new_to_pin   = graph::MakePinId(ti->second, static_cast<uint32_t>(to_pin_idx));

        if (g.CanConnect(new_from_pin, new_to_pin)) {
            uint64_t cid = g.Connect(new_from_pin, new_to_pin);
            if (cid != 0)
                cmds.push_back(std::make_unique<graph::ConnectCmd>(
                    g.connections.back(), std::nullopt));
        }
    }

    if (!cmds.empty()) {
        undo_.Push(std::make_unique<graph::MacroCmd>(std::move(cmds), "Paste"));
        project::Project::Get().MarkDirty();
    }
}

void GraphEditorPanel::DuplicateSelected(graph::ScriptGraph& g) {
    CopySelected(g);
    PasteClipboard(g);
}

// ── Alignment ─────────────────────────────────────────────────────────────────
// mode: 0=left,1=right,2=top,3=bottom,4=centre-h,5=centre-v,6=distrib-h,7=distrib-v

void GraphEditorPanel::AlignSelected(graph::ScriptGraph& g, int mode) {
    // collect node IDs with their positions
    auto sel_ids = GetSelectedNodeIds();
    if (sel_ids.size() < 2) return;

    ne::SetCurrentEditor(ctx_);

    struct NodePos { uint64_t id; float x, y, w, h; };
    std::vector<NodePos> nps;
    nps.reserve(sel_ids.size());
    for (uint64_t nid : sel_ids) {
        ImVec2 pos  = ne::GetNodePosition(ne::NodeId(nid));
        ImVec2 size = ne::GetNodeSize(ne::NodeId(nid));
        nps.push_back({nid, pos.x, pos.y, size.x, size.y});
    }
    ne::SetCurrentEditor(nullptr);

    // Compute targets
    float ref_left   = nps[0].x;
    float ref_right  = nps[0].x + nps[0].w;
    float ref_top    = nps[0].y;
    float ref_bottom = nps[0].y + nps[0].h;
    for (const auto& np : nps) {
        ref_left   = std::min(ref_left,   np.x);
        ref_right  = std::max(ref_right,  np.x + np.w);
        ref_top    = std::min(ref_top,    np.y);
        ref_bottom = std::max(ref_bottom, np.y + np.h);
    }

    // Sort for distribute
    std::vector<NodePos> sorted_h = nps, sorted_v = nps;
    std::sort(sorted_h.begin(), sorted_h.end(), [](const NodePos& a, const NodePos& b) { return a.x < b.x; });
    std::sort(sorted_v.begin(), sorted_v.end(), [](const NodePos& a, const NodePos& b) { return a.y < b.y; });

    std::vector<std::unique_ptr<graph::ICommand>> cmds;

    ne::SetCurrentEditor(ctx_);
    for (auto& np : nps) {
        float new_x = np.x, new_y = np.y;
        switch (mode) {
        case 0: new_x = ref_left;               break; // left
        case 1: new_x = ref_right - np.w;       break; // right
        case 2: new_y = ref_top;                break; // top
        case 3: new_y = ref_bottom - np.h;      break; // bottom
        case 4: new_x = (ref_left + ref_right  - np.w) * 0.5f; break; // centre-h
        case 5: new_y = (ref_top  + ref_bottom - np.h) * 0.5f; break; // centre-v
        case 6: { // distribute-h
            float total_w = 0;
            for (const auto& n2 : nps) total_w += n2.w;
            float gap = (ref_right - ref_left - total_w) / float(nps.size() - 1);
            float cx = ref_left;
            for (size_t i = 0; i < sorted_h.size(); ++i) {
                if (sorted_h[i].id == np.id) { new_x = cx; break; }
                cx += sorted_h[i].w + gap;
            }
            break;
        }
        case 7: { // distribute-v
            float total_h = 0;
            for (const auto& n2 : nps) total_h += n2.h;
            float gap = (ref_bottom - ref_top - total_h) / float(nps.size() - 1);
            float cy = ref_top;
            for (size_t i = 0; i < sorted_v.size(); ++i) {
                if (sorted_v[i].id == np.id) { new_y = cy; break; }
                cy += sorted_v[i].h + gap;
            }
            break;
        }
        }
        if (new_x != np.x || new_y != np.y) {
            cmds.push_back(std::make_unique<graph::MoveNodeCmd>(np.id,
                np.x, np.y, new_x, new_y));
            ne::SetNodePosition(ne::NodeId(np.id), ImVec2(new_x, new_y));
        }
    }
    ne::SetCurrentEditor(nullptr);

    if (!cmds.empty()) {
        undo_.Push(std::make_unique<graph::MacroCmd>(std::move(cmds), "Align"));
        project::Project::Get().MarkDirty();
    }
}

} // namespace ui
