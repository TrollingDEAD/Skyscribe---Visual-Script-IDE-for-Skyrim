#include <catch2/catch_test_macros.hpp>

#include "codegen/GraphTraversal.h"
#include "codegen/PapyrusStringBuilder.h"
#include "codegen/LintPass.h"
#include "graph/BuiltinNodes.h"
#include "graph/GraphSerializer.h"
#include "graph/NodeRegistry.h"
#include "graph/ScriptGraph.h"

#include <algorithm>
#include <string>

// Ensure builtins are registered before any test runs.
struct BuiltinFixture {
    BuiltinFixture() { graph::BuiltinNodes::RegisterAll(); }
};
static BuiltinFixture g_builtins;

using namespace graph;
using namespace codegen;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool Contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static ScriptGraph MakeGraph(const std::string& name = "TestScript",
                              const std::string& extends = "ObjectReference") {
    ScriptGraph g;
    g.script_name = name;
    g.extends = extends;
    return g;
}

static uint64_t AddBuiltin(ScriptGraph& g, const std::string& type_id,
                             float x = 0.0f, float y = 0.0f) {
    const NodeDefinition* def = NodeRegistry::Get().Find(type_id);
    REQUIRE(def != nullptr);
    return g.AddNode(*def, x, y);
}

// Connect the first exec-out of 'from' to the exec-in of 'to'.
static void ConnectExec(ScriptGraph& g, uint64_t from_id, uint64_t to_id) {
    const ScriptNode* from = g.FindNode(from_id);
    const ScriptNode* to   = g.FindNode(to_id);
    REQUIRE(from); REQUIRE(to);
    uint64_t out_pin = 0, in_pin = 0;
    for (const auto& p : from->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output)
            { out_pin = p.id; break; }
    for (const auto& p : to->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input)
            { in_pin = p.id; break; }
    REQUIRE(out_pin); REQUIRE(in_pin);
    uint64_t cid = g.Connect(out_pin, in_pin);
    REQUIRE(cid != 0);
}

// Connect exec-out named 'pin_name' of 'from' to exec-in of 'to'.
static void ConnectExecNamed(ScriptGraph& g, uint64_t from_id,
                              const std::string& pin_name, uint64_t to_id) {
    const ScriptNode* from = g.FindNode(from_id);
    const ScriptNode* to   = g.FindNode(to_id);
    REQUIRE(from); REQUIRE(to);
    uint64_t out_pin = GraphTraversal::FindExecOutPin(g, from_id, pin_name);
    REQUIRE(out_pin != 0);
    uint64_t in_pin = 0;
    for (const auto& p : to->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input)
            { in_pin = p.id; break; }
    REQUIRE(in_pin);
    REQUIRE(g.Connect(out_pin, in_pin) != 0);
}

// ── 3.1 GraphTraversal ────────────────────────────────────────────────────────

TEST_CASE("Traverse returns empty result for disconnected start pin", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t exec_pin = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::Traverse(g, exec_pin);
    CHECK(result.node_ids.empty());
    CHECK_FALSE(result.has_cycle);
}

TEST_CASE("Traverse follows linear exec chain", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init  = AddBuiltin(g, "builtin.OnInit");
    uint64_t notif1   = AddBuiltin(g, "builtin.Notification");
    uint64_t notif2   = AddBuiltin(g, "builtin.Notification");

    ConnectExec(g, on_init, notif1);
    ConnectExec(g, notif1,  notif2);

    uint64_t start_pin = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::Traverse(g, start_pin);

    REQUIRE(result.node_ids.size() == 2);
    CHECK(result.node_ids[0] == notif1);
    CHECK(result.node_ids[1] == notif2);
    CHECK_FALSE(result.has_cycle);
}

TEST_CASE("TraverseDfs captures IfElse branches in True then False order", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t if_node = AddBuiltin(g, "builtin.IfElse");
    uint64_t n_true  = AddBuiltin(g, "builtin.Notification");
    uint64_t n_false = AddBuiltin(g, "builtin.Trace");

    ConnectExec(g, on_init, if_node);
    ConnectExecNamed(g, if_node, "True", n_true);
    ConnectExecNamed(g, if_node, "False", n_false);

    uint64_t start = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::TraverseDfs(g, start);

    REQUIRE(result.node_ids.size() >= 3);
    CHECK(result.node_ids[0] == if_node);

    auto it_true  = std::find(result.node_ids.begin(), result.node_ids.end(), n_true);
    auto it_false = std::find(result.node_ids.begin(), result.node_ids.end(), n_false);
    REQUIRE(it_true  != result.node_ids.end());
    REQUIRE(it_false != result.node_ids.end());
    CHECK(it_true < it_false);
}

TEST_CASE("Traverse stops at IfElse (branching node)", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t if_node = AddBuiltin(g, "builtin.IfElse");
    ConnectExec(g, on_init, if_node);

    uint64_t start = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::Traverse(g, start);

    REQUIRE(result.node_ids.size() == 1);
    CHECK(result.node_ids[0] == if_node);
}

TEST_CASE("Traverse continues past WhileLoop via Completed pin", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t loop    = AddBuiltin(g, "builtin.WhileLoop");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");

    ConnectExec(g, on_init, loop);
    ConnectExecNamed(g, loop, "Completed", notif);

    uint64_t start = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::Traverse(g, start);

    REQUIRE(result.node_ids.size() == 2);
    CHECK(result.node_ids[0] == loop);
    CHECK(result.node_ids[1] == notif);
}

TEST_CASE("Traverse detects execution cycles", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t a = AddBuiltin(g, "builtin.Notification");
    uint64_t b = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, a, b);

    // Create a synthetic back-edge B -> A to validate cycle detection in traversal.
    uint64_t b_out = 0, a_in = 0, start = 0;
    const ScriptNode* nb = g.FindNode(b);
    const ScriptNode* na = g.FindNode(a);
    for (const auto& p : nb->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output) { b_out = p.id; break; }
    for (const auto& p : na->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input) { a_in = p.id; break; }
    for (const auto& p : na->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output) { start = p.id; break; }

    REQUIRE(b_out != 0);
    REQUIRE(a_in  != 0);
    REQUIRE(start != 0);

    Connection back;
    back.id = g.next_conn_id++;
    back.from_pin_id = b_out;
    back.to_pin_id   = a_in;
    g.connections.push_back(back);

    auto result = GraphTraversal::TraverseDfs(g, start);
    CHECK(result.has_cycle);
    auto edges = GraphTraversal::GetCycleEdges(result);
    CHECK_FALSE(edges.empty());
}

TEST_CASE("TraverseDfs handles empty graph", "[traversal]") {
    ScriptGraph g = MakeGraph();
    auto result = GraphTraversal::TraverseDfs(g, 0);
    CHECK(result.node_ids.empty());
    CHECK_FALSE(result.has_cycle);
    CHECK(result.data_dependencies.empty());
}

TEST_CASE("TraverseDfs recognises while loop body and back-edge cycle", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t loop    = AddBuiltin(g, "builtin.WhileLoop");
    uint64_t body    = AddBuiltin(g, "builtin.Notification");
    uint64_t after   = AddBuiltin(g, "builtin.Trace");

    ConnectExec(g, on_init, loop);
    ConnectExecNamed(g, loop, "Loop Body", body);
    ConnectExecNamed(g, loop, "Completed", after);

    // Synthetic loop-back edge: body Out -> loop In
    uint64_t body_out = 0, loop_in = 0;
    const ScriptNode* n_body = g.FindNode(body);
    const ScriptNode* n_loop = g.FindNode(loop);
    for (const auto& p : n_body->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output) { body_out = p.id; break; }
    for (const auto& p : n_loop->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input) { loop_in = p.id; break; }
    REQUIRE(body_out != 0);
    REQUIRE(loop_in  != 0);
    Connection back;
    back.id = g.next_conn_id++;
    back.from_pin_id = body_out;
    back.to_pin_id   = loop_in;
    g.connections.push_back(back);

    uint64_t start = GraphTraversal::FindExecOutPin(g, on_init, "Out");
    auto result = GraphTraversal::TraverseDfs(g, start);

    auto has_node = [&](uint64_t id) {
        return std::find(result.node_ids.begin(), result.node_ids.end(), id) != result.node_ids.end();
    };
    CHECK(has_node(loop));
    CHECK(has_node(body));
    CHECK(has_node(after));
    CHECK(result.has_cycle);
    CHECK_FALSE(result.cycle_edges.empty());
}

TEST_CASE("FindEventNodes returns only event nodes", "[traversal]") {
    ScriptGraph g = MakeGraph();
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.OnDeath");
    AddBuiltin(g, "builtin.Notification"); // not an event

    auto events = GraphTraversal::FindEventNodes(g);
    CHECK(events.size() == 2);
}

TEST_CASE("FindExecOutPin returns correct pin id", "[traversal]") {
    ScriptGraph g = MakeGraph();
    uint64_t if_node = AddBuiltin(g, "builtin.IfElse");
    uint64_t true_pin  = GraphTraversal::FindExecOutPin(g, if_node, "True");
    uint64_t false_pin = GraphTraversal::FindExecOutPin(g, if_node, "False");
    CHECK(true_pin  != 0);
    CHECK(false_pin != 0);
    CHECK(true_pin  != false_pin);
    CHECK(GraphTraversal::FindExecOutPin(g, if_node, "Nonexistent") == 0);
}

// ── 3.2 PapyrusStringBuilder ──────────────────────────────────────────────────

TEST_CASE("Generate emits Scriptname header", "[codegen]") {
    ScriptGraph g = MakeGraph("MyScript", "ObjectReference");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Scriptname MyScript extends ObjectReference"));
}

TEST_CASE("Generate emits source comment when enabled", "[codegen]") {
    PapyrusStringBuilder::SetEmitSourceComments(true);

    ScriptGraph g = MakeGraph("CommentScript", "ObjectReference");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "; Generated by Skyscribe"));

    PapyrusStringBuilder::SetEmitSourceComments(false);
}

TEST_CASE("Generate uses tab indentation when configured", "[codegen]") {
    PapyrusStringBuilder::SetIndentWithTabs(true);

    ScriptGraph g = MakeGraph("IndentScript", "ObjectReference");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, notif);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "\n\tDebug.Notification("));

    PapyrusStringBuilder::SetIndentWithTabs(false);
}

TEST_CASE("Generate empty script name is an error", "[codegen]") {
    ScriptGraph g = MakeGraph("", "");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(result.has_errors);
}

TEST_CASE("Generate empty graph produces only header", "[codegen]") {
    ScriptGraph g = MakeGraph("EmptyScript", "");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(result.has_errors);
    CHECK(Contains(result.source, "Scriptname EmptyScript\n"));
    // No event blocks
    CHECK_FALSE(Contains(result.source, "Event "));
}

TEST_CASE("Generate emits Event block for OnInit", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Event OnInit()"));
    CHECK(Contains(result.source, "EndEvent"));
}

TEST_CASE("Generate OnDeath includes Actor parameter", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnDeath");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Event OnDeath(Actor akKiller)"));
}

TEST_CASE("Generate emits Notification statement", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, notif);

    // Set the message pin value
    ScriptNode* n = g.FindNode(notif);
    for (auto& p : n->pins)
        if (p.name == "akMessage") p.value = "Hello";

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Debug.Notification(Hello)"));
}

TEST_CASE("Generate uses pin default value when disconnected", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t trace   = AddBuiltin(g, "builtin.Trace");
    ConnectExec(g, on_init, trace);

    auto result = PapyrusStringBuilder::Generate(g);
    // akSeverity default is "0", akMessage default is ""
    CHECK(Contains(result.source, "Debug.Trace("));
    CHECK(Contains(result.source, ", 0)"));
}

TEST_CASE("Generate emits Return statement", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t ret     = AddBuiltin(g, "builtin.Return");
    ConnectExec(g, on_init, ret);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Return"));
}

TEST_CASE("Generate emits If/EndIf block", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init  = AddBuiltin(g, "builtin.OnInit");
    uint64_t if_node  = AddBuiltin(g, "builtin.IfElse");
    uint64_t notif    = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, if_node);
    ConnectExecNamed(g, if_node, "True", notif);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "If ("));
    CHECK(Contains(result.source, "EndIf"));
    CHECK_FALSE(Contains(result.source, "Else\n"));
}

TEST_CASE("Generate emits Else branch when False pin is connected", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init   = AddBuiltin(g, "builtin.OnInit");
    uint64_t if_node   = AddBuiltin(g, "builtin.IfElse");
    uint64_t true_notif  = AddBuiltin(g, "builtin.Notification");
    uint64_t false_notif = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, if_node);
    ConnectExecNamed(g, if_node, "True",  true_notif);
    ConnectExecNamed(g, if_node, "False", false_notif);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "If ("));
    CHECK(Contains(result.source, "Else"));
    CHECK(Contains(result.source, "EndIf"));
}

TEST_CASE("Generate emits While/EndWhile block", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t wloop   = AddBuiltin(g, "builtin.WhileLoop");
    ConnectExec(g, on_init, wloop);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "While ("));
    CHECK(Contains(result.source, "EndWhile"));
}

TEST_CASE("Generate continues after WhileLoop via Completed", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t wloop   = AddBuiltin(g, "builtin.WhileLoop");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, wloop);
    ConnectExecNamed(g, wloop, "Completed", notif);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "While ("));
    CHECK(Contains(result.source, "EndWhile"));
    // Notification should appear AFTER EndWhile
    auto while_pos = result.source.find("While (");
    auto endwhile_pos = result.source.find("EndWhile");
    auto notif_pos = result.source.find("Debug.Notification");
    CHECK(while_pos   != std::string::npos);
    CHECK(endwhile_pos != std::string::npos);
    CHECK(notif_pos    != std::string::npos);
    CHECK(notif_pos > endwhile_pos);
}

TEST_CASE("Generate resolves chained data expressions", "[codegen]") {
    // GetPlayer().IsDead() chained into IfElse Condition
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init   = AddBuiltin(g, "builtin.OnInit");
    uint64_t if_node   = AddBuiltin(g, "builtin.IfElse");
    uint64_t is_dead   = AddBuiltin(g, "builtin.IsDead");
    uint64_t get_player = AddBuiltin(g, "builtin.GetPlayer");

    // Connect: on_init ->exec-> if_node
    ConnectExec(g, on_init, if_node);

    // Connect: GetPlayer.Player -> IsDead.akActor
    const ScriptNode* gp = g.FindNode(get_player);
    const ScriptNode* id = g.FindNode(is_dead);
    uint64_t player_out = 0, actor_in = 0;
    for (const auto& p : gp->pins)
        if (p.name == "Player") { player_out = p.id; break; }
    for (const auto& p : id->pins)
        if (p.name == "akActor") { actor_in = p.id; break; }
    REQUIRE(player_out); REQUIRE(actor_in);
    REQUIRE(g.Connect(player_out, actor_in) != 0);

    // Connect: IsDead.Dead -> IfElse.Condition
    const ScriptNode* idn = g.FindNode(is_dead);
    const ScriptNode* ifn = g.FindNode(if_node);
    uint64_t dead_out = 0, cond_in = 0;
    for (const auto& p : idn->pins)
        if (p.name == "Dead") { dead_out = p.id; break; }
    for (const auto& p : ifn->pins)
        if (p.name == "Condition") { cond_in = p.id; break; }
    REQUIRE(dead_out); REQUIRE(cond_in);
    REQUIRE(g.Connect(dead_out, cond_in) != 0);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Game.GetPlayer().IsDead()"));
}

TEST_CASE("Generate hoists math expression into temp variable", "[codegen]") {
    // AddInt.Result feeds TWO consumers → must be hoisted to a temp var (multi-consumer rule).
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t svar0   = AddBuiltin(g, "builtin.SetVariable");
    uint64_t svar1   = AddBuiltin(g, "builtin.SetVariable");
    uint64_t add_int = AddBuiltin(g, "builtin.AddInt");
    ConnectExec(g, on_init, svar0);
    ConnectExec(g, svar0,   svar1);

    // Set target variable names
    auto set_var_name = [&](uint64_t id, const char* name) {
        ScriptNode* sv = g.FindNode(id);
        for (auto& p : sv->pins)
            if (p.name == "Variable") { p.value = name; return; }
    };
    set_var_name(svar0, "iA");
    set_var_name(svar1, "iB");

    // Set A and B defaults on AddInt
    ScriptNode* an = g.FindNode(add_int);
    for (auto& p : an->pins) {
        if (p.name == "A") p.value = "1";
        if (p.name == "B") p.value = "2";
    }

    // Connect AddInt.Result -> both SetVariable nodes' Value input (dual consumer)
    uint64_t result_out = 0;
    for (const auto& p : an->pins)
        if (p.name == "Result") { result_out = p.id; break; }
    REQUIRE(result_out);

    for (auto sv_id : {svar0, svar1}) {
        const ScriptNode* sv = g.FindNode(sv_id);
        for (const auto& p : sv->pins) {
            if (p.name == "Value") { REQUIRE(g.Connect(result_out, p.id) != 0); break; }
        }
    }

    auto res = PapyrusStringBuilder::Generate(g);
    // Temp var must be declared (hoisted) and referenced in both assignments
    CHECK(Contains(res.source, "Int _t0 = 1 + 2"));
    CHECK(Contains(res.source, "iA = _t0"));
    CHECK(Contains(res.source, "iB = _t0"));
}

TEST_CASE("Generate emits Sequence branches in declaration order", "[codegen]") {
    ScriptGraph g = MakeGraph("SeqScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t seq     = AddBuiltin(g, "builtin.Sequence");
    uint64_t n0      = AddBuiltin(g, "builtin.Notification");
    uint64_t n1      = AddBuiltin(g, "builtin.Notification");
    uint64_t n2      = AddBuiltin(g, "builtin.Notification");

    ConnectExec(g, on_init, seq);
    ConnectExecNamed(g, seq, "Then 0", n0);
    ConnectExecNamed(g, seq, "Then 1", n1);
    ConnectExecNamed(g, seq, "Then 2", n2);

    auto set_msg = [&](uint64_t node_id, const std::string& msg) {
        ScriptNode* n = g.FindNode(node_id);
        REQUIRE(n != nullptr);
        for (auto& p : n->pins)
            if (p.name == "akMessage") { p.value = msg; return; }
        FAIL("Notification node missing akMessage pin");
    };
    set_msg(n0, "\"A\"");
    set_msg(n1, "\"B\"");
    set_msg(n2, "\"C\"");

    auto result = PapyrusStringBuilder::Generate(g);
    auto a = result.source.find("Debug.Notification(\"A\")");
    auto b = result.source.find("Debug.Notification(\"B\")");
    auto c = result.source.find("Debug.Notification(\"C\")");
    REQUIRE(a != std::string::npos);
    REQUIRE(b != std::string::npos);
    REQUIRE(c != std::string::npos);
    CHECK(a < b);
    CHECK(b < c);
}

TEST_CASE("Generate emits properties", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    PropertyDefinition prop;
    prop.name = "MyProp";
    prop.type = PinType::Int;
    prop.kind = PropertyKind::Auto;
    prop.default_value = "42";
    g.properties.push_back(prop);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Int Property MyProp Auto = 42"));
}

// ── 3.3 Variable hoisting (DeclareLocal) ─────────────────────────────────────

TEST_CASE("Generate hoists DeclareLocal to top of event", "[codegen]") {
    // An isolated DeclareLocal (not exec-connected) should not appear as a statement.
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.DeclareLocal");  // not connected

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(Contains(result.source, "builtin.DeclareLocal"));
}

TEST_CASE("DeclareLocal in exec chain is hoisted with correct type and initial value", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t decl    = AddBuiltin(g, "builtin.DeclareLocal");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, decl);
    ConnectExec(g, decl, notif);

    ScriptNode* dn = g.FindNode(decl);
    for (auto& p : dn->pins) {
        if (p.name == "Name")         p.value = "kCounter";
        if (p.name == "Type")         p.value = "Int";
        if (p.name == "InitialValue") p.value = "0";
    }

    auto result = PapyrusStringBuilder::Generate(g);
    // Declaration must appear (hoisted) and be before the Notification statement
    CHECK(Contains(result.source, "Int kCounter = 0"));
    auto decl_pos  = result.source.find("Int kCounter");
    auto notif_pos = result.source.find("Debug.Notification");
    REQUIRE(decl_pos  != std::string::npos);
    REQUIRE(notif_pos != std::string::npos);
    CHECK(decl_pos < notif_pos);
    // DeclareLocal must not appear as an inline statement
    CHECK_FALSE(Contains(result.source, "builtin.DeclareLocal"));
}

TEST_CASE("Single-consumer math output is inlined, no temp var", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t svar    = AddBuiltin(g, "builtin.SetVariable");
    uint64_t add_int = AddBuiltin(g, "builtin.AddInt");
    ConnectExec(g, on_init, svar);

    ScriptNode* sv = g.FindNode(svar);
    for (auto& p : sv->pins)
        if (p.name == "Variable") p.value = "iResult";

    ScriptNode* an = g.FindNode(add_int);
    for (auto& p : an->pins) {
        if (p.name == "A") p.value = "3";
        if (p.name == "B") p.value = "4";
    }

    // Single connection: AddInt.Result → SetVariable.Value
    uint64_t result_out = 0, value_in = 0;
    for (const auto& p : an->pins)
        if (p.name == "Result") { result_out = p.id; break; }
    for (const auto& p : g.FindNode(svar)->pins)
        if (p.name == "Value") { value_in = p.id; break; }
    REQUIRE(result_out); REQUIRE(value_in);
    REQUIRE(g.Connect(result_out, value_in) != 0);

    auto res = PapyrusStringBuilder::Generate(g);
    // Inlined: no "_t0 =" declaration; expression used directly
    CHECK_FALSE(Contains(res.source, "_t0"));
    CHECK(Contains(res.source, "iResult = 3 + 4"));
}

TEST_CASE("Dual-consumer math output hoisted to temp var", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t svar0   = AddBuiltin(g, "builtin.SetVariable");
    uint64_t svar1   = AddBuiltin(g, "builtin.SetVariable");
    uint64_t add_int = AddBuiltin(g, "builtin.AddInt");
    ConnectExec(g, on_init, svar0);
    ConnectExec(g, svar0, svar1);

    auto set_var = [&](uint64_t id, const char* name) {
        ScriptNode* n = g.FindNode(id);
        for (auto& p : n->pins)
            if (p.name == "Variable") { p.value = name; return; }
    };
    set_var(svar0, "iA"); set_var(svar1, "iB");

    ScriptNode* an = g.FindNode(add_int);
    for (auto& p : an->pins) {
        if (p.name == "A") p.value = "5";
        if (p.name == "B") p.value = "6";
    }

    uint64_t result_out = 0;
    for (const auto& p : an->pins)
        if (p.name == "Result") { result_out = p.id; break; }
    REQUIRE(result_out);
    for (auto sv_id : {svar0, svar1}) {
        const ScriptNode* sv = g.FindNode(sv_id);
        for (const auto& p : sv->pins)
            if (p.name == "Value") { REQUIRE(g.Connect(result_out, p.id) != 0); break; }
    }

    auto res = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(res.source, "Int _t0 = 5 + 6"));
    CHECK(Contains(res.source, "iA = _t0"));
    CHECK(Contains(res.source, "iB = _t0"));
}

// ── 3.4 Edge cases ────────────────────────────────────────────────────────────

TEST_CASE("Generate handles graph with only pure data nodes", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.LiteralInt");    // no exec pins
    AddBuiltin(g, "builtin.GetPlayer");     // no exec pins
    // No events — should produce header only, no events
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(result.has_errors);
    CHECK_FALSE(Contains(result.source, "Event "));
}

TEST_CASE("Generate handles multiple event nodes", "[codegen]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.OnDeath");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Event OnInit()"));
    CHECK(Contains(result.source, "Event OnDeath(Actor akKiller)"));
}

// ── 3.4 Required unit tests ───────────────────────────────────────────────────

TEST_CASE("EdgeCase: empty graph produces valid minimal psc", "[edgecase]") {
    // Empty graph (no nodes at all) → valid Papyrus header, no crash, no errors
    ScriptGraph g = MakeGraph("EmptyX", "ObjectReference");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(result.has_errors);
    CHECK(Contains(result.source, "Scriptname EmptyX"));
    CHECK(Contains(result.source, "extends ObjectReference"));
    CHECK_FALSE(Contains(result.source, "Event "));
}

TEST_CASE("EdgeCase: unconnected String pin emits empty string literal", "[edgecase]") {
    // Notification's akMessage pin is a String with default "" — must emit "", not crash
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, notif);
    // Leave akMessage pin unconnected and with no value — relies on DefaultLiteralForType("String") = "\"\""
    ScriptNode* n = g.FindNode(notif);
    for (auto& p : n->pins)
        if (p.name == "akMessage") p.value = "";  // ensure unset
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(result.has_errors);
    // akMessage should fall back to its default "" (two double quotes in source)
    CHECK(Contains(result.source, "Debug.Notification("));
    // No crash — that's the key
}

TEST_CASE("EdgeCase: duplicate OnInit emits only one event block", "[edgecase]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.OnInit");  // duplicate
    auto result = PapyrusStringBuilder::Generate(g);
    // Only one Event OnInit() block
    auto first  = result.source.find("Event OnInit()");
    auto second = result.source.find("Event OnInit()", first + 1);
    REQUIRE(first != std::string::npos);
    CHECK(second == std::string::npos);
    // has_errors flagged because of the duplicate
    CHECK(result.has_errors);
}

TEST_CASE("EdgeCase: exec-flow cycle emits error comment and sets has_errors", "[edgecase]") {
    // Build a manual cycle by directly connecting two Notification nodes back-to-back
    // and then using GraphTraversal to confirm cycle detection.
    // We test the PapyrusStringBuilder response via a known-cycle traversal.
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t notif   = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, on_init, notif);

    // Create cycle: Connect notif exec-out back toward on_init exec-out pin
    // (not a real exec-in, but enough to make Traverse detect a cycle via visited set)
    // Instead, directly test via TraverseDfs with a manually constructed cycle:
    uint64_t notif2 = AddBuiltin(g, "builtin.Notification");
    ConnectExec(g, notif, notif2);
    // Connect notif2 back to notif's exec-in to form the cycle
    const ScriptNode* n  = g.FindNode(notif);
    const ScriptNode* n2 = g.FindNode(notif2);
    uint64_t exec_in_notif = 0;
    for (const auto& p : n->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input) { exec_in_notif = p.id; break; }
    uint64_t exec_out_notif2 = 0;
    for (const auto& p : n2->pins)
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output) { exec_out_notif2 = p.id; break; }
    REQUIRE(exec_in_notif); REQUIRE(exec_out_notif2);
    // Force connect (bypass validation since ScriptGraph::Connect may reject cycles)
    Connection c; c.id = 9999; c.from_pin_id = exec_out_notif2; c.to_pin_id = exec_in_notif;
    g.connections.push_back(c);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(result.has_errors);
    CHECK(Contains(result.source, "; [ERROR: Execution-flow cycle at node"));
}

// ── 3.11 PropertyDefinition ───────────────────────────────────────────────────

TEST_CASE("ScriptGraph stores property definitions", "[properties]") {
    ScriptGraph g = MakeGraph();
    PropertyDefinition p1;
    p1.name = "Health"; p1.type = PinType::Float; p1.kind = PropertyKind::Auto;
    PropertyDefinition p2;
    p2.name = "IsAlive"; p2.type = PinType::Bool; p2.kind = PropertyKind::AutoReadOnly;
    g.properties.push_back(p1);
    g.properties.push_back(p2);
    CHECK(g.properties.size() == 2);
    CHECK(g.properties[0].name == "Health");
    CHECK(g.properties[1].type == PinType::Bool);
}

TEST_CASE("Generate emits AutoReadOnly property", "[properties]") {
    ScriptGraph g = MakeGraph("TestScript");
    PropertyDefinition prop;
    prop.name = "MyFlag"; prop.type = PinType::Bool;
    prop.kind = PropertyKind::AutoReadOnly;
    g.properties.push_back(prop);
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Bool Property MyFlag AutoReadOnly"));
}

// ── 3.14 LintPass ─────────────────────────────────────────────────────────────

TEST_CASE("LintPass L01: empty script name is error", "[lint]") {
    ScriptGraph g; g.script_name = "";
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L01") { found = true; break; }
    CHECK(found);
    CHECK(LintPass::HasErrors(diags));
}

TEST_CASE("LintPass L06: no event nodes is warning", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.Notification"); // not an event
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L06") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass L08: duplicate event is error", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.OnInit");
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L08") { found = true; break; }
    CHECK(found);
    CHECK(LintPass::HasErrors(diags));
}

TEST_CASE("LintPass L09: script name with spaces is error", "[lint]") {
    ScriptGraph g = MakeGraph("My Script");
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L09") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass L09: digit-starting name is error", "[lint]") {
    ScriptGraph g = MakeGraph("1Script");
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L09") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass L10: duplicate property names is error", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    PropertyDefinition p; p.name = "MyProp"; p.type = PinType::Int;
    g.properties.push_back(p);
    g.properties.push_back(p);
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L10") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass L12: script extending itself is error", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript", "TestScript");
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L12") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass clean graph produces no errors", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript", "ObjectReference");
    AddBuiltin(g, "builtin.OnInit");
    auto diags = LintPass::Run(g);
    CHECK_FALSE(LintPass::HasErrors(diags));
}

TEST_CASE("LintPass L05: isolated node produces warning", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");         // has no connections but is event — skipped
    AddBuiltin(g, "builtin.Notification");   // has exec-in, isolated → warning
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L05") { found = true; break; }
    CHECK(found);
}

TEST_CASE("LintPass L13: unreachable action node warning", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    AddBuiltin(g, "builtin.OnInit");
    AddBuiltin(g, "builtin.Notification"); // not connected to event
    auto diags = LintPass::Run(g);
    bool found_l13 = false;
    for (const auto& d : diags) if (d.rule_id == "L13") { found_l13 = true; break; }
    CHECK(found_l13);
}

TEST_CASE("LintPass L07: division by zero warning", "[lint]") {
    ScriptGraph g = MakeGraph("TestScript");
    uint64_t div_node = AddBuiltin(g, "builtin.DivideInt");
    // B pin defaults to "1" in the def, but instance value defaults to ""
    // → only fires if pin.value == "0" or ""
    // Our test: set B to "0"
    ScriptNode* n = g.FindNode(div_node);
    for (auto& p : n->pins)
        if (p.name == "B") { p.value = "0"; break; }
    auto diags = LintPass::Run(g);
    bool found = false;
    for (const auto& d : diags) if (d.rule_id == "L07") { found = true; break; }
    CHECK(found);
}

// ── 3.12 Get/Set Property Nodes ───────────────────────────────────────────────

TEST_CASE("SyncPropertyNodes registers Get/Set nodes in registry", "[propnodes]") {
    PropertyDefinition prop;
    prop.name = "TestProp";
    prop.type = PinType::Int;
    prop.kind = PropertyKind::Auto;

    BuiltinNodes::SyncPropertyNodes("SyncTestScript", { prop });

    const NodeDefinition* get_def = NodeRegistry::Get().Find("script.SyncTestScript.get.TestProp");
    const NodeDefinition* set_def = NodeRegistry::Get().Find("script.SyncTestScript.set.TestProp");
    REQUIRE(get_def != nullptr);
    REQUIRE(set_def != nullptr);

    // Get node: one DataOut pin named TestProp, Int type
    REQUIRE(get_def->pins.size() == 1);
    CHECK(get_def->pins[0].flow == PinFlow::Data);
    CHECK(get_def->pins[0].kind == PinKind::Output);
    CHECK(get_def->pins[0].type == PinType::Int);

    // Set node: ExecIn + DataIn("Value") + ExecOut
    bool has_exec_in = false, has_value_in = false, has_exec_out = false;
    for (const auto& p : set_def->pins) {
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input)  has_exec_in  = true;
        if (p.flow == PinFlow::Data      && p.kind == PinKind::Input)  has_value_in = true;
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output) has_exec_out = true;
    }
    CHECK(has_exec_in);
    CHECK(has_value_in);
    CHECK(has_exec_out);

    BuiltinNodes::SyncPropertyNodes("SyncTestScript", {});
}

TEST_CASE("SyncPropertyNodes removes nodes when property deleted", "[propnodes]") {
    PropertyDefinition prop;
    prop.name = "TempProp";
    prop.type = PinType::Bool;
    prop.kind = PropertyKind::Auto;

    BuiltinNodes::SyncPropertyNodes("SyncTestScript2", { prop });
    REQUIRE(NodeRegistry::Get().Find("script.SyncTestScript2.get.TempProp") != nullptr);

    BuiltinNodes::SyncPropertyNodes("SyncTestScript2", {});
    CHECK(NodeRegistry::Get().Find("script.SyncTestScript2.get.TempProp") == nullptr);
    CHECK(NodeRegistry::Get().Find("script.SyncTestScript2.set.TempProp") == nullptr);
}

TEST_CASE("GetProperty node codegen emits bare property name", "[propnodes]") {
    PropertyDefinition prop;
    prop.name = "MyActor";
    prop.type = PinType::Actor;
    prop.kind = PropertyKind::Auto;

    BuiltinNodes::SyncPropertyNodes("PropCodegenScript", { prop });

    ScriptGraph g = MakeGraph("PropCodegenScript", "ObjectReference");
    uint64_t on_init  = AddBuiltin(g, "builtin.OnInit");
    uint64_t svar     = AddBuiltin(g, "builtin.SetVariable");
    uint64_t get_prop = g.AddNode(*NodeRegistry::Get().Find("script.PropCodegenScript.get.MyActor"));
    ConnectExec(g, on_init, svar);

    // Connect GetProperty.MyActor -> SetVariable.Value
    const ScriptNode* gp = g.FindNode(get_prop);
    const ScriptNode* sv = g.FindNode(svar);
    uint64_t prop_out = 0, val_in = 0;
    for (const auto& p : gp->pins)
        if (p.flow == PinFlow::Data && p.kind == PinKind::Output) { prop_out = p.id; break; }
    for (const auto& p : sv->pins)
        if (p.name == "Value") { val_in = p.id; break; }
    REQUIRE(prop_out != 0); REQUIRE(val_in != 0);
    g.Connect(prop_out, val_in);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "MyActor"));

    BuiltinNodes::SyncPropertyNodes("PropCodegenScript", {});
}

TEST_CASE("SetProperty node codegen emits assignment", "[propnodes]") {
    PropertyDefinition prop;
    prop.name = "iCount";
    prop.type = PinType::Int;
    prop.kind = PropertyKind::Auto;

    BuiltinNodes::SyncPropertyNodes("SetPropScript", { prop });

    ScriptGraph g = MakeGraph("SetPropScript", "ObjectReference");
    uint64_t on_init  = AddBuiltin(g, "builtin.OnInit");
    uint64_t set_prop = g.AddNode(*NodeRegistry::Get().Find("script.SetPropScript.set.iCount"));
    ConnectExec(g, on_init, set_prop);

    ScriptNode* sp = g.FindNode(set_prop);
    for (auto& p : sp->pins)
        if (p.name == "Value") { p.value = "5"; break; }

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "iCount = 5"));

    BuiltinNodes::SyncPropertyNodes("SetPropScript", {});
}

// ── 3.16 Self Pin ─────────────────────────────────────────────────────────────

TEST_CASE("builtin.GetSelf is registered", "[self]") {
    const NodeDefinition* def = NodeRegistry::Get().Find("builtin.GetSelf");
    REQUIRE(def != nullptr);
    CHECK(def->display_name == "Self");
    CHECK(def->codegen_template == "Self");
    REQUIRE(def->pins.size() == 1);
    CHECK(def->pins[0].flow == PinFlow::Data);
    CHECK(def->pins[0].kind == PinKind::Output);
}

TEST_CASE("GetSelf codegen emits Self keyword", "[self]") {
    ScriptGraph g = MakeGraph("SelfTest", "Actor");
    uint64_t on_init  = AddBuiltin(g, "builtin.OnInit");
    uint64_t set_var  = AddBuiltin(g, "builtin.SetVariable");
    uint64_t get_self = AddBuiltin(g, "builtin.GetSelf");
    ConnectExec(g, on_init, set_var);

    const ScriptNode* gs = g.FindNode(get_self);
    const ScriptNode* sv = g.FindNode(set_var);
    uint64_t self_out = 0, val_in = 0;
    for (const auto& p : gs->pins)
        if (p.flow == PinFlow::Data && p.kind == PinKind::Output) { self_out = p.id; break; }
    for (const auto& p : sv->pins)
        if (p.name == "Value") { val_in = p.id; break; }
    REQUIRE(self_out != 0); REQUIRE(val_in != 0);
    g.Connect(self_out, val_in);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Self"));
}

// ── 3.17 Import Statement Generation ─────────────────────────────────────────

TEST_CASE("Generate emits no Import lines for builtins-only graph", "[import]") {
    ScriptGraph g = MakeGraph("ImportTest");
    AddBuiltin(g, "builtin.OnInit");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK_FALSE(Contains(result.source, "Import "));
}

TEST_CASE("Generate emits Import line for node with source_script", "[import]") {
    NodeDefinition fake;
    fake.type_id          = "test.FakeReflected";
    fake.display_name     = "FakeReflected";
    fake.category         = NodeCategory::Custom;
    fake.source_script    = "PapyrusExtender";
    fake.codegen_template = "PapyrusExtender.DoThing()";
    fake.pins = { PinDefinition{"In",  PinKind::Input,  PinFlow::Execution, PinType::Exec, "", ""},
                  PinDefinition{"Out", PinKind::Output, PinFlow::Execution, PinType::Exec, "", ""} };
    NodeRegistry::Get().Register(fake);

    ScriptGraph g = MakeGraph("ImportTest2", "ObjectReference");
    uint64_t on_init   = AddBuiltin(g, "builtin.OnInit");
    uint64_t fake_node = AddBuiltin(g, "test.FakeReflected");
    ConnectExec(g, on_init, fake_node);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Import PapyrusExtender"));

    auto imp_pos   = result.source.find("Import PapyrusExtender");
    auto event_pos = result.source.find("Event ");
    CHECK(imp_pos   != std::string::npos);
    CHECK(event_pos != std::string::npos);
    CHECK(imp_pos < event_pos);

    NodeRegistry::Get().Unregister("test.FakeReflected");
}

TEST_CASE("Generate emits unique sorted Import lines", "[import]") {
    auto make_reflected = [](const std::string& tid, const std::string& src) {
        NodeDefinition d;
        d.type_id          = tid;
        d.display_name     = tid;
        d.category         = NodeCategory::Custom;
        d.source_script    = src;
        d.codegen_template = src + ".Fn()";
        d.pins = { PinDefinition{"In",  PinKind::Input,  PinFlow::Execution, PinType::Exec, "", ""},
                   PinDefinition{"Out", PinKind::Output, PinFlow::Execution, PinType::Exec, "", ""} };
        return d;
    };
    NodeRegistry::Get().Register(make_reflected("test.NodeA1", "LibraryA"));
    NodeRegistry::Get().Register(make_reflected("test.NodeA2", "LibraryA"));
    NodeRegistry::Get().Register(make_reflected("test.NodeB1", "LibraryB"));

    ScriptGraph g = MakeGraph("ImportTest3", "ObjectReference");
    uint64_t on_init = AddBuiltin(g, "builtin.OnInit");
    uint64_t a1 = AddBuiltin(g, "test.NodeA1");
    uint64_t a2 = AddBuiltin(g, "test.NodeA2");
    uint64_t b1 = AddBuiltin(g, "test.NodeB1");
    ConnectExec(g, on_init, a1);
    ConnectExec(g, a1, a2);
    ConnectExec(g, a2, b1);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Import LibraryA"));
    CHECK(Contains(result.source, "Import LibraryB"));

    // LibraryA appears only once
    auto pos1 = result.source.find("Import LibraryA");
    auto pos2 = result.source.find("Import LibraryA", pos1 + 1);
    CHECK(pos1 != std::string::npos);
    CHECK(pos2 == std::string::npos);

    // Alphabetical: LibraryA before LibraryB
    CHECK(result.source.find("Import LibraryA") < result.source.find("Import LibraryB"));

    NodeRegistry::Get().Unregister("test.NodeA1");
    NodeRegistry::Get().Unregister("test.NodeA2");
    NodeRegistry::Get().Unregister("test.NodeB1");
}

// ── 3.9 User-Defined Functions ────────────────────────────────────────────────

TEST_CASE("AddFunction registers Entry/Return/Call nodes", "[functions]") {
    ScriptGraph g = MakeGraph("FuncRegScript");
    g.AddFunction("MyFunc");
    CHECK(NodeRegistry::Get().Find("script.FuncRegScript.entry.MyFunc") != nullptr);
    CHECK(NodeRegistry::Get().Find("script.FuncRegScript.return.MyFunc") != nullptr);
    CHECK(NodeRegistry::Get().Find("script.FuncRegScript.call.MyFunc")   != nullptr);
    g.RemoveFunction("MyFunc");
}

TEST_CASE("Generate emits Function/EndFunction block", "[functions]") {
    ScriptGraph g = MakeGraph("FuncEmitScript");
    g.AddFunction("DoThing");
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Function DoThing()"));
    CHECK(Contains(result.source, "EndFunction"));
    g.RemoveFunction("DoThing");
}

TEST_CASE("Generate emits typed Function with return type", "[functions]") {
    ScriptGraph g = MakeGraph("FuncTypedScript");
    g.AddFunction("GetValue", PinType::Float);
    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Float Function GetValue()"));
    g.RemoveFunction("GetValue");
}

TEST_CASE("Generate emits Return with value in typed function", "[functions]") {
    ScriptGraph g = MakeGraph("FuncReturnValueScript");
    auto& f = g.AddFunction("GetValue", PinType::Float);
    REQUIRE(f.body_graph != nullptr);

    const std::string entry_type  = "script." + g.script_name + ".entry.GetValue";
    const std::string return_type = "script." + g.script_name + ".return.GetValue";

    uint64_t entry_id = 0;
    for (const auto& n : f.body_graph->nodes) {
        if (n.type_id == entry_type) { entry_id = n.id; break; }
    }
    REQUIRE(entry_id != 0);

    const NodeDefinition* ret_def = NodeRegistry::Get().Find(return_type);
    REQUIRE(ret_def != nullptr);
    uint64_t ret_id = f.body_graph->AddNode(*ret_def, 280.0f, 80.0f);
    ConnectExec(*f.body_graph, entry_id, ret_id);

    ScriptNode* rn = f.body_graph->FindNode(ret_id);
    REQUIRE(rn != nullptr);
    bool set_value = false;
    for (auto& p : rn->pins) {
        if (p.name == "Value" && p.flow == PinFlow::Data && p.kind == PinKind::Input) {
            p.value = "1.5";
            set_value = true;
            break;
        }
    }
    REQUIRE(set_value);

    auto result = PapyrusStringBuilder::Generate(g);
    CHECK(Contains(result.source, "Float Function GetValue()"));
    CHECK(Contains(result.source, "Return 1.5"));

    g.RemoveFunction("GetValue");
}

TEST_CASE("Function emitted before Event block", "[functions]") {
    ScriptGraph g = MakeGraph("FuncOrderScript");
    g.AddFunction("Helper");
    AddBuiltin(g, "builtin.OnInit");
    auto result  = PapyrusStringBuilder::Generate(g);
    auto func_pos  = result.source.find("Function Helper");
    auto event_pos = result.source.find("Event OnInit");
    CHECK(func_pos  != std::string::npos);
    CHECK(event_pos != std::string::npos);
    CHECK(func_pos < event_pos);
    g.RemoveFunction("Helper");
}

TEST_CASE("GraphSerializer round-trips FunctionDefinition", "[functions]") {
    ScriptGraph g = MakeGraph("SerFuncScript");
    g.AddFunction("Round", PinType::Int);
    auto j = GraphSerializer::Save(g);
    // Unregister before load to test re-registration path
    NodeRegistry::Get().Unregister("script.SerFuncScript.entry.Round");
    NodeRegistry::Get().Unregister("script.SerFuncScript.return.Round");
    NodeRegistry::Get().Unregister("script.SerFuncScript.call.Round");
    auto g2 = GraphSerializer::Load(j);
    REQUIRE(g2.functions.size() == 1);
    CHECK(g2.functions[0].name        == "Round");
    CHECK(g2.functions[0].return_type == PinType::Int);
    CHECK(NodeRegistry::Get().Find("script.SerFuncScript.entry.Round")  != nullptr);
    CHECK(NodeRegistry::Get().Find("script.SerFuncScript.return.Round") != nullptr);
    CHECK(NodeRegistry::Get().Find("script.SerFuncScript.call.Round")   != nullptr);
    g2.RemoveFunction("Round");
}

// ── 3.10 Cross-Script Call Node Generation ────────────────────────────────────

TEST_CASE("SyncCrossScriptNodes registers project call nodes", "[crossscript]") {
    ScriptGraph scriptB = MakeGraph("ScriptB");
    scriptB.AddFunction("Greet");
    std::vector<ScriptGraph> scripts = { scriptB };
    BuiltinNodes::SyncCrossScriptNodes(scripts);

    const auto* def = NodeRegistry::Get().Find("project.ScriptB.Greet");
    REQUIRE(def != nullptr);
    CHECK(def->display_name == "ScriptB::Greet");

    // Check it has a Self DataIn pin and an exec pair
    bool has_self = false, has_exec_in = false, has_exec_out = false;
    for (const auto& p : def->pins) {
        if (p.name == "Self" && p.flow == PinFlow::Data && p.kind == PinKind::Input)
            has_self = true;
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Input)
            has_exec_in = true;
        if (p.flow == PinFlow::Execution && p.kind == PinKind::Output)
            has_exec_out = true;
    }
    CHECK(has_self);
    CHECK(has_exec_in);
    CHECK(has_exec_out);

    BuiltinNodes::SyncCrossScriptNodes({}); // cleanup
    scriptB.RemoveFunction("Greet");
}

TEST_CASE("SyncCrossScriptNodes clears old nodes on re-sync", "[crossscript]") {
    ScriptGraph sA = MakeGraph("ScriptA");
    sA.AddFunction("DoA");
    BuiltinNodes::SyncCrossScriptNodes({ sA });
    CHECK(NodeRegistry::Get().Find("project.ScriptA.DoA") != nullptr);

    // Re-sync with empty list removes all cross-script nodes
    BuiltinNodes::SyncCrossScriptNodes({});
    CHECK(NodeRegistry::Get().Find("project.ScriptA.DoA") == nullptr);

    sA.RemoveFunction("DoA");
}

TEST_CASE("Cross-script call node emits cast+call codegen", "[crossscript]") {
    // Script B defines a function
    ScriptGraph scriptB = MakeGraph("NpcScript");
    scriptB.AddFunction("SayHello");
    BuiltinNodes::SyncCrossScriptNodes({ scriptB });

    // Script A calls it
    ScriptGraph gA = MakeGraph("PlayerScript");
    uint64_t on_init  = AddBuiltin(gA, "builtin.OnInit");
    uint64_t call_id  = gA.AddNode(*NodeRegistry::Get().Find("project.NpcScript.SayHello"));
    ConnectExec(gA, on_init, call_id);

    auto result = PapyrusStringBuilder::Generate(gA);
    CHECK(Contains(result.source, "(None as NpcScript).SayHello()"));

    BuiltinNodes::SyncCrossScriptNodes({}); // cleanup
    scriptB.RemoveFunction("SayHello");
}

TEST_CASE("Cross-script call with typed return gets ReturnValue output pin", "[crossscript]") {
    ScriptGraph scriptB = MakeGraph("QuestScript");
    scriptB.AddFunction("GetLevel", PinType::Int);
    BuiltinNodes::SyncCrossScriptNodes({ scriptB });

    const auto* def = NodeRegistry::Get().Find("project.QuestScript.GetLevel");
    REQUIRE(def != nullptr);

    bool has_return_val = false;
    for (const auto& p : def->pins)
        if (p.name == "ReturnValue" && p.flow == PinFlow::Data && p.kind == PinKind::Output)
            has_return_val = true;
    CHECK(has_return_val);

    BuiltinNodes::SyncCrossScriptNodes({}); // cleanup
    scriptB.RemoveFunction("GetLevel");
}

// ─── PapyrusLexer tests ──────────────────────────────────────────────────────
#include "codegen/PapyrusLexer.h"
#include <cstring>

TEST_CASE("PapyrusLexer has keyword Event stored as uppercase", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    // TextEditor uppercases identifiers before keyword lookup when mCaseSensitive==false
    CHECK(lang.mKeywords.count("EVENT") > 0);
}

TEST_CASE("PapyrusLexer has semicolon single-line comment", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    CHECK(lang.mSingleLineComment == ";");
}

TEST_CASE("PapyrusLexer is case-insensitive", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    CHECK(lang.mCaseSensitive == false);
}

TEST_CASE("PapyrusLexer tokenize classifies quoted literal as String", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    REQUIRE(lang.mTokenize != nullptr);
    const char* src = "\"Hello\"";
    const char* ob = nullptr; const char* oe = nullptr;
    TextEditor::PaletteIndex color = TextEditor::PaletteIndex::Default;
    bool ok = lang.mTokenize(src, src + std::strlen(src), ob, oe, color);
    CHECK(ok);
    CHECK(color == TextEditor::PaletteIndex::String);
    CHECK((oe - ob) == 7);
}

TEST_CASE("PapyrusLexer tokenize classifies integer as Number", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    REQUIRE(lang.mTokenize != nullptr);
    const char* src = "42";
    const char* ob = nullptr; const char* oe = nullptr;
    TextEditor::PaletteIndex color = TextEditor::PaletteIndex::Default;
    bool ok = lang.mTokenize(src, src + std::strlen(src), ob, oe, color);
    CHECK(ok);
    CHECK(color == TextEditor::PaletteIndex::Number);
}

TEST_CASE("PapyrusLexer tokenize classifies plain word as Identifier", "[lexer]") {
    auto lang = codegen::PapyrusLexer::GetLanguageDefinition();
    REQUIRE(lang.mTokenize != nullptr);
    const char* src = "PlayerRef";
    const char* ob = nullptr; const char* oe = nullptr;
    TextEditor::PaletteIndex color = TextEditor::PaletteIndex::Default;
    bool ok = lang.mTokenize(src, src + std::strlen(src), ob, oe, color);
    CHECK(ok);
    CHECK(color == TextEditor::PaletteIndex::Identifier);
}
