#include "graph/UndoStack.h"

#include <algorithm>

namespace graph {

// ── Helper: restore node into graph without reassigning ID ───────────────────

static void RestoreNode(ScriptGraph& g, const ScriptNode& node) {
    if (g.FindNode(node.id)) return; // already present
    if (g.next_node_id <= node.id)
        g.next_node_id = node.id + 1;
    g.nodes.push_back(node);
}

static void RestoreConnection(ScriptGraph& g, const Connection& c) {
    auto it = std::find_if(g.connections.begin(), g.connections.end(),
        [&c](const Connection& x) { return x.id == c.id; });
    if (it != g.connections.end()) return; // already present
    if (g.next_conn_id <= c.id)
        g.next_conn_id = c.id + 1;
    g.connections.push_back(c);
}

// ── AddNodeCmd ────────────────────────────────────────────────────────────────

void AddNodeCmd::Execute(ScriptGraph& g) {
    RestoreNode(g, node_);
}

void AddNodeCmd::Undo(ScriptGraph& g) {
    for (const auto& p : node_.pins)
        g.DisconnectPin(p.id);
    auto it = std::find_if(g.nodes.begin(), g.nodes.end(),
        [this](const ScriptNode& n) { return n.id == node_.id; });
    if (it != g.nodes.end()) g.nodes.erase(it);
}

// ── RemoveNodeCmd ─────────────────────────────────────────────────────────────

void RemoveNodeCmd::Execute(ScriptGraph& g) {
    g.RemoveNode(node_.id);
}

void RemoveNodeCmd::Undo(ScriptGraph& g) {
    RestoreNode(g, node_);
    for (const auto& c : connections_)
        RestoreConnection(g, c);
}

// ── MoveNodeCmd ───────────────────────────────────────────────────────────────

void MoveNodeCmd::Execute(ScriptGraph& g) {
    ScriptNode* n = g.FindNode(node_id_);
    if (n) { n->pos_x = new_x_; n->pos_y = new_y_; }
}

void MoveNodeCmd::Undo(ScriptGraph& g) {
    ScriptNode* n = g.FindNode(node_id_);
    if (n) { n->pos_x = old_x_; n->pos_y = old_y_; }
}

// ── ConnectCmd ────────────────────────────────────────────────────────────────

void ConnectCmd::Execute(ScriptGraph& g) {
    if (replaced_) g.Disconnect(replaced_->id);
    RestoreConnection(g, conn_);
}

void ConnectCmd::Undo(ScriptGraph& g) {
    g.Disconnect(conn_.id);
    if (replaced_) RestoreConnection(g, *replaced_);
}

// ── DisconnectCmd ─────────────────────────────────────────────────────────────

void DisconnectCmd::Execute(ScriptGraph& g) {
    g.Disconnect(conn_.id);
}

void DisconnectCmd::Undo(ScriptGraph& g) {
    if (!g.FindPin(conn_.from_pin_id) || !g.FindPin(conn_.to_pin_id)) return;
    RestoreConnection(g, conn_);
}

// ── SetPinValueCmd ────────────────────────────────────────────────────────────

void SetPinValueCmd::Execute(ScriptGraph& g) {
    Pin* p = g.FindPin(pin_id_);
    if (p) p->value = new_val_;
}

void SetPinValueCmd::Undo(ScriptGraph& g) {
    Pin* p = g.FindPin(pin_id_);
    if (p) p->value = old_val_;
}

// ── MacroCmd ──────────────────────────────────────────────────────────────────

void MacroCmd::Execute(ScriptGraph& g) {
    for (auto& c : cmds_) c->Execute(g);
}

void MacroCmd::Undo(ScriptGraph& g) {
    for (auto it = cmds_.rbegin(); it != cmds_.rend(); ++it)
        (*it)->Undo(g);
}

// ── AddFunctionCmd ────────────────────────────────────────────────────────────

void AddFunctionCmd::Execute(ScriptGraph& g) {
    // Re-add via AddFunction (re-registers nodes), then restore body_graph content.
    g.AddFunction(func_.name, func_.return_type, func_.parameters, func_.is_global);
    FunctionDefinition* f = g.FindFunction(func_.name);
    if (f && func_.body_graph)
        *f->body_graph = *func_.body_graph;
}

void AddFunctionCmd::Undo(ScriptGraph& g) {
    g.RemoveFunction(func_.name);
}

// ── DeleteFunctionCmd ─────────────────────────────────────────────────────────

void DeleteFunctionCmd::Execute(ScriptGraph& g) {
    // Save current state before removing (for redo correctness if called again)
    FunctionDefinition* f = g.FindFunction(func_.name);
    if (f) func_ = *f;
    g.RemoveFunction(func_.name);
}

void DeleteFunctionCmd::Undo(ScriptGraph& g) {
    g.AddFunction(func_.name, func_.return_type, func_.parameters, func_.is_global);
    FunctionDefinition* f = g.FindFunction(func_.name);
    if (f && func_.body_graph)
        *f->body_graph = *func_.body_graph;
}

// ── UndoStack ─────────────────────────────────────────────────────────────────

void UndoStack::Push(std::unique_ptr<ICommand> cmd) {
    redo_.clear();
    undo_.push_back(std::move(cmd));
    if (undo_.size() > MAX_HISTORY)
        undo_.pop_front();
}

void UndoStack::Undo(ScriptGraph& g) {
    if (undo_.empty()) return;
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    cmd->Undo(g);
    redo_.push_back(std::move(cmd));
}

void UndoStack::Redo(ScriptGraph& g) {
    if (redo_.empty()) return;
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    cmd->Execute(g);
    undo_.push_back(std::move(cmd));
}

std::string UndoStack::UndoDescription() const {
    return undo_.empty() ? "" : undo_.back()->Description();
}

std::string UndoStack::RedoDescription() const {
    return redo_.empty() ? "" : redo_.back()->Description();
}

void UndoStack::Clear() {
    undo_.clear();
    redo_.clear();
}

} // namespace graph
