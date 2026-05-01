#include <catch2/catch_test_macros.hpp>
#include "graph/UndoStack.h"
#include "graph/ScriptGraph.h"
#include "graph/NodeDefinition.h"

using namespace graph;

// ── Fixture ───────────────────────────────────────────────────────────────────

static NodeDefinition MakeExecInOut(const char* type_id) {
    NodeDefinition def;
    def.type_id = type_id; def.display_name = type_id; def.category = NodeCategory::Debug;
    PinDefinition ei, eo;
    ei.name = "In";  ei.kind = PinKind::Input;  ei.flow = PinFlow::Execution; ei.type = PinType::Exec;
    eo.name = "Out"; eo.kind = PinKind::Output; eo.flow = PinFlow::Execution; eo.type = PinType::Exec;
    def.pins.push_back(ei);
    def.pins.push_back(eo);
    return def;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("AddNodeCmd: undo removes node; redo restores it", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.A");

    uint64_t id = g.AddNode(def);
    stack.Push(std::make_unique<AddNodeCmd>(*g.FindNode(id)));

    REQUIRE(g.nodes.size() == 1);

    stack.Undo(g);
    REQUIRE(g.nodes.empty());
    REQUIRE(stack.CanRedo());

    stack.Redo(g);
    REQUIRE(g.nodes.size() == 1);
    REQUIRE(g.nodes[0].id == id);
}

TEST_CASE("ConnectCmd: undo removes connection; redo restores it", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.B");
    uint64_t nA = g.AddNode(def);
    uint64_t nB = g.AddNode(def);

    // nA.ExecOut → nB.ExecIn
    uint64_t from = MakePinId(nA, 1);
    uint64_t to   = MakePinId(nB, 0);
    REQUIRE(g.Connect(from, to) != 0);
    stack.Push(std::make_unique<ConnectCmd>(g.connections[0]));

    REQUIRE(g.connections.size() == 1);

    stack.Undo(g);
    REQUIRE(g.connections.empty());

    stack.Redo(g);
    REQUIRE(g.connections.size() == 1);
}

TEST_CASE("UndoStack caps history at 100 entries", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.C");

    for (int i = 0; i < 101; ++i) {
        uint64_t id = g.AddNode(def);
        stack.Push(std::make_unique<AddNodeCmd>(*g.FindNode(id)));
    }

    size_t count = 0;
    while (stack.CanUndo()) { stack.Undo(g); ++count; }
    REQUIRE(count == 100); // oldest entry was dropped
}

TEST_CASE("MacroCmd: single undo removes all sub-commands", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.D");

    auto macro = std::make_unique<MacroCmd>("Add 3 Nodes");
    for (int i = 0; i < 3; ++i) {
        uint64_t id = g.AddNode(def);
        macro->Add(std::make_unique<AddNodeCmd>(*g.FindNode(id)));
    }
    REQUIRE(g.nodes.size() == 3);
    stack.Push(std::move(macro));

    stack.Undo(g);
    REQUIRE(g.nodes.empty());

    stack.Redo(g);
    REQUIRE(g.nodes.size() == 3);
}

TEST_CASE("RemoveNodeCmd: undo restores node and connections", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.E");
    uint64_t nA = g.AddNode(def);
    uint64_t nB = g.AddNode(def);
    g.Connect(MakePinId(nA, 1), MakePinId(nB, 0));
    REQUIRE(g.connections.size() == 1);

    // Capture snapshot for undo before removing
    ScriptNode snap = *g.FindNode(nA);
    std::vector<Connection> removed_conns;
    for (auto* c : g.ConnectionsForPin(MakePinId(nA, 1))) removed_conns.push_back(*c);

    g.RemoveNode(nA);
    REQUIRE(g.nodes.size() == 1);
    REQUIRE(g.connections.empty());

    stack.Push(std::make_unique<RemoveNodeCmd>(snap, removed_conns));
    stack.Undo(g);

    REQUIRE(g.nodes.size() == 2);
    REQUIRE(g.connections.size() == 1);
}

TEST_CASE("SetPinValueCmd: undo restores old value", "[undo]") {
    ScriptGraph g;
    UndoStack   stack;
    auto def = MakeExecInOut("undo.F");
    uint64_t n  = g.AddNode(def);
    uint64_t pid = MakePinId(n, 0);

    Pin* p = g.FindPin(pid);
    REQUIRE(p != nullptr);
    p->value = "hello";

    stack.Push(std::make_unique<SetPinValueCmd>(pid, "", "hello"));

    stack.Undo(g);
    REQUIRE(g.FindPin(pid)->value == "");

    stack.Redo(g);
    REQUIRE(g.FindPin(pid)->value == "hello");
}
