#include "ui/GraphEditorPanel.h"
#include "graph/NodeRegistry.h"
#include "graph/PinColorMap.h"
#include "graph/UndoStack.h"
#include "project/Project.h"
#include "app/Logger.h"

#include <imgui_node_editor.h>
#include <imgui_internal.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace ne = ax::NodeEditor;

namespace {

// ── Category helpers ──────────────────────────────────────────────────────────

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

    // ── Canvas ────────────────────────────────────────────────────────────────
    RenderCanvas(g);

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

    ne::End();
    ne::SetCurrentEditor(nullptr);

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

} // namespace ui
