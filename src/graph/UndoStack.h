#pragma once

#include "graph/ScriptGraph.h"

#include <memory>
#include <deque>
#include <vector>
#include <string>
#include <optional>

namespace graph {

// ── Command interface ──────────────────────────────────────────────────────────

struct ICommand {
    virtual ~ICommand() = default;

    // Execute (also the "redo" path). Called on Redo.
    virtual void Execute(ScriptGraph& g) = 0;

    // Revert the action. Called on Undo.
    virtual void Undo(ScriptGraph& g) = 0;

    virtual std::string Description() const = 0;
};

// ── Concrete commands ──────────────────────────────────────────────────────────

struct AddNodeCmd : ICommand {
    ScriptNode node_;
    explicit AddNodeCmd(ScriptNode n) : node_(std::move(n)) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Add Node"; }
};

struct RemoveNodeCmd : ICommand {
    ScriptNode              node_;
    std::vector<Connection> connections_;
    RemoveNodeCmd(ScriptNode n, std::vector<Connection> c)
        : node_(std::move(n)), connections_(std::move(c)) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Delete Node"; }
};

struct MoveNodeCmd : ICommand {
    uint64_t node_id_;
    float    old_x_, old_y_, new_x_, new_y_;
    MoveNodeCmd(uint64_t id, float ox, float oy, float nx, float ny)
        : node_id_(id), old_x_(ox), old_y_(oy), new_x_(nx), new_y_(ny) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Move Node"; }
};

struct ConnectCmd : ICommand {
    Connection                conn_;
    std::optional<Connection> replaced_; // connection displaced by single-consumer rule
    ConnectCmd(Connection c, std::optional<Connection> r = std::nullopt)
        : conn_(c), replaced_(r) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Connect Pins"; }
};

struct DisconnectCmd : ICommand {
    Connection conn_;
    explicit DisconnectCmd(Connection c) : conn_(c) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Disconnect"; }
};

struct SetPinValueCmd : ICommand {
    uint64_t    pin_id_;
    std::string old_val_, new_val_;
    SetPinValueCmd(uint64_t pid, std::string ov, std::string nv)
        : pin_id_(pid), old_val_(std::move(ov)), new_val_(std::move(nv)) {}
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return "Edit Pin Value"; }
};

// Wraps multiple commands into a single undoable step (e.g. multi-node paste).
struct MacroCmd : ICommand {
    std::vector<std::unique_ptr<ICommand>> cmds_;
    std::string                            desc_;
    explicit MacroCmd(std::string d) : desc_(std::move(d)) {}
    void Add(std::unique_ptr<ICommand> c) { cmds_.push_back(std::move(c)); }
    void Execute(ScriptGraph& g) override;
    void Undo(ScriptGraph& g)    override;
    std::string Description()    const override { return desc_; }
};

// ── UndoStack ─────────────────────────────────────────────────────────────────

class UndoStack {
public:
    static constexpr size_t MAX_HISTORY = 100;

    // Record a command that has ALREADY been applied to the graph.
    // Clears the redo list.
    void Push(std::unique_ptr<ICommand> cmd);

    // Undo the last command. No-op if empty.
    void Undo(ScriptGraph& g);

    // Redo the last undone command. No-op if redo list empty.
    void Redo(ScriptGraph& g);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }

    std::string UndoDescription() const;
    std::string RedoDescription() const;

    void Clear();

private:
    std::deque<std::unique_ptr<ICommand>>  undo_;
    std::vector<std::unique_ptr<ICommand>> redo_;
};

} // namespace graph
