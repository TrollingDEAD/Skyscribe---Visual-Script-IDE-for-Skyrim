#pragma once

#include "graph/ScriptGraph.h"
#include "graph/UndoStack.h"

#include <imgui.h>
#include <imgui_node_editor.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui {

// Clipboard payload for copy/paste operations.
struct ClipboardPayload {
    std::vector<graph::ScriptNode>   nodes;
    std::vector<graph::Connection>   internal_connections;
};

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
    void RenderNodePicker(graph::ScriptGraph& g, ImVec2 canvas_pos, uint64_t from_pin_id = 0);
    void RenderNodeContextMenu(graph::ScriptGraph& g);

    // Copy/paste helpers
    void CopySelected(graph::ScriptGraph& g);
    void PasteClipboard(graph::ScriptGraph& g);
    void CutSelected(graph::ScriptGraph& g);
    void DuplicateSelected(graph::ScriptGraph& g);
    void AlignSelected(graph::ScriptGraph& g, int mode); // 0=left,1=right,2=top,3=bottom,4=ch,5=cv,6=dh,7=dv

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

    // Node picker state
    bool        picker_open_         = false;
    ImVec2      picker_canvas_pos_   = {};
    uint64_t    picker_from_pin_id_  = 0; // 0 = not pin-drag triggered
    char        picker_search_[256]  = {};

    // Node context menu
    uint64_t    ctx_node_id_         = 0; // node under right-click

    // Clipboard
    ClipboardPayload clipboard_;
    bool             has_clipboard_  = false;
    int              paste_offset_   = 0; // incremented on each paste

    // Function tab state (task 3.9)
    int          active_func_tab_     = -1; // -1 = event graph
    std::string  func_tab_script_;         // detect script switch
    std::unordered_map<std::string, ax::NodeEditor::EditorContext*> func_ctxs_;
    std::unordered_set<uint64_t>   func_positioned_nodes_;
    char         func_name_buf_[129]  = {};
    int          func_return_type_idx_ = 0; // 0 = None
    bool         func_add_dialog_open_ = false;
    // Rename state
    char         func_rename_buf_[129] = {};
    int          func_rename_idx_      = -1; // which function is being renamed
};

} // namespace ui
