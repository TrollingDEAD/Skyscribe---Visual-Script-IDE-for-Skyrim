#pragma once

#include "graph/ScriptGraph.h"
#include "graph/UndoStack.h"

#include <imgui.h>
#include <imgui_node_editor.h>
#include <string>
#include <unordered_set>

namespace ui {

class GraphEditorPanel {
public:
    GraphEditorPanel()  = default;
    ~GraphEditorPanel();

    void Render();

    // Call before Project::Save() to flush imgui-node-editor positions into
    // the ScriptGraph data so they are persisted.
    void SyncNodePositions();

private:
    void RenderCanvas(graph::ScriptGraph& g);
    void DrawNode(graph::ScriptGraph& g, graph::ScriptNode& node);
    void DrawPinIcon(const graph::Pin& p);
    void HandleCreate(graph::ScriptGraph& g);
    void HandleDelete(graph::ScriptGraph& g);
    void HandleKeyboardShortcuts(graph::ScriptGraph& g);
    void AcceptPaletteDrop(graph::ScriptGraph& g);

    ax::NodeEditor::EditorContext* ctx_    = nullptr;
    std::string                    last_script_name_; // detect active graph switch
    std::unordered_set<uint64_t>   positioned_nodes_; // nodes with restored positions

    graph::UndoStack               undo_;

    // Pending drop from ToolPalettePanel (set by AcceptPaletteDrop)
    std::string pending_type_id_;
    ImVec2      pending_drop_pos_ = {};

    // Script identity bar editing buffers
    char name_buf_[129]    = {};
    char extends_buf_[129] = {};
};

} // namespace ui
