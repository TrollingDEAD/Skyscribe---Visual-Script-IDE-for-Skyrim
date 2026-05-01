#include <catch2/catch_test_macros.hpp>
#include "graph/ScriptGraph.h"
#include "graph/NodeDefinition.h"

using namespace graph;

// ── Helpers ───────────────────────────────────────────────────────────────────

static NodeDefinition MakeExecNode(const std::string& type_id) {
    NodeDefinition def;
    def.type_id      = type_id;
    def.display_name = type_id;
    def.category     = NodeCategory::Event;
    // exec-out pin
    PinDefinition exec_out;
    exec_out.name = "ExecOut";
    exec_out.kind = PinKind::Output;
    exec_out.flow = PinFlow::Execution;
    exec_out.type = PinType::Exec;
    def.pins.push_back(exec_out);
    return def;
}

static NodeDefinition MakeExecInOutNode(const std::string& type_id) {
    NodeDefinition def;
    def.type_id      = type_id;
    def.display_name = type_id;
    def.category     = NodeCategory::Debug;
    PinDefinition exec_in;
    exec_in.name = "ExecIn";
    exec_in.kind = PinKind::Input;
    exec_in.flow = PinFlow::Execution;
    exec_in.type = PinType::Exec;
    PinDefinition exec_out;
    exec_out.name = "ExecOut";
    exec_out.kind = PinKind::Output;
    exec_out.flow = PinFlow::Execution;
    exec_out.type = PinType::Exec;
    def.pins.push_back(exec_in);
    def.pins.push_back(exec_out);
    return def;
}

static NodeDefinition MakeDataNode(const std::string& type_id, PinType out_type, PinType in_type) {
    NodeDefinition def;
    def.type_id = type_id;
    def.category = NodeCategory::Variable;
    PinDefinition out;
    out.name = "Out"; out.kind = PinKind::Output; out.flow = PinFlow::Data; out.type = out_type;
    PinDefinition in;
    in.name  = "In";  in.kind  = PinKind::Input;  in.flow  = PinFlow::Data; in.type  = in_type;
    def.pins.push_back(out);
    def.pins.push_back(in);
    return def;
}

// ── 2.1 Graph data model ──────────────────────────────────────────────────────

TEST_CASE("AddNode assigns unique IDs", "[graph]") {
    ScriptGraph g;
    auto def = MakeExecNode("test.A");
    uint64_t id1 = g.AddNode(def);
    uint64_t id2 = g.AddNode(def);
    uint64_t id3 = g.AddNode(def);
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(g.nodes.size() == 3);
}

TEST_CASE("RemoveNode removes node and does not reuse ID", "[graph]") {
    ScriptGraph g;
    auto def = MakeExecNode("test.A");
    uint64_t id1 = g.AddNode(def);
    uint64_t id2 = g.AddNode(def);
    g.RemoveNode(id1);
    REQUIRE(g.nodes.size() == 1);
    REQUIRE(g.nodes[0].id == id2);
    // Next added node must not reuse id1
    uint64_t id3 = g.AddNode(def);
    REQUIRE(id3 != id1);
    REQUIRE(id3 != id2);
}

TEST_CASE("RemoveNode also removes its connections", "[graph]") {
    ScriptGraph g;
    auto def = MakeExecInOutNode("test.B");
    uint64_t nA = g.AddNode(def);
    uint64_t nB = g.AddNode(def);
    uint64_t nC = g.AddNode(def);

    // nA.ExecOut -> nB.ExecIn -> nB.ExecOut -> nC.ExecIn
    uint64_t pA_out = MakePinId(nA, 1); // ExecOut is pin index 1? No — index 0=ExecIn, 1=ExecOut
    // Wait: MakeExecInOutNode pushes ExecIn first (index 0), ExecOut second (index 1)
    uint64_t pA_execout = MakePinId(nA, 1);
    uint64_t pB_execin  = MakePinId(nB, 0);
    uint64_t pB_execout = MakePinId(nB, 1);
    uint64_t pC_execin  = MakePinId(nC, 0);

    REQUIRE(g.Connect(pA_execout, pB_execin) != 0);
    REQUIRE(g.Connect(pB_execout, pC_execin) != 0);
    REQUIRE(g.connections.size() == 2);

    g.RemoveNode(nB);
    REQUIRE(g.nodes.size() == 2);
    REQUIRE(g.connections.empty()); // both connections involved nB
}

TEST_CASE("FindNode and FindPin return correct pointers", "[graph]") {
    ScriptGraph g;
    auto def = MakeExecNode("test.C");
    uint64_t id = g.AddNode(def, 10.0f, 20.0f);
    ScriptNode* n = g.FindNode(id);
    REQUIRE(n != nullptr);
    REQUIRE(n->pos_x == 10.0f);
    REQUIRE(n->pos_y == 20.0f);
    REQUIRE(n->type_id == "test.C");

    uint64_t pin_id = MakePinId(id, 0);
    const Pin* p = g.FindPin(pin_id);
    REQUIRE(p != nullptr);
    REQUIRE(p->flow == PinFlow::Execution);
}

TEST_CASE("RemoveMiddleNode leaves only unrelated connections", "[graph]") {
    ScriptGraph g;
    auto def = MakeExecInOutNode("test.D");
    uint64_t nA = g.AddNode(def);
    uint64_t nB = g.AddNode(def);
    uint64_t nC = g.AddNode(def);

    uint64_t pA_out = MakePinId(nA, 1);
    uint64_t pB_in  = MakePinId(nB, 0);
    uint64_t pB_out = MakePinId(nB, 1);
    uint64_t pC_in  = MakePinId(nC, 0);

    g.Connect(pA_out, pB_in);
    g.Connect(pB_out, pC_in);
    g.RemoveNode(nB);

    REQUIRE(g.connections.empty()); // both connections referenced nB
    REQUIRE(g.nodes.size() == 2);
}

// ── 2.3 Connection enforcement ────────────────────────────────────────────────

TEST_CASE("Exec-to-Data connection rejected", "[graph][connections]") {
    ScriptGraph g;
    auto exec_def = MakeExecNode("test.Exec");
    auto data_def = MakeDataNode("test.Data", PinType::Int, PinType::Int);
    uint64_t nExec = g.AddNode(exec_def);
    uint64_t nData = g.AddNode(data_def);

    uint64_t exec_out = MakePinId(nExec, 0); // Exec Output
    uint64_t data_in  = MakePinId(nData, 1); // Data Input
    REQUIRE(g.Connect(exec_out, data_in) == 0);
}

TEST_CASE("Data-to-Exec connection rejected", "[graph][connections]") {
    ScriptGraph g;
    auto exec_def = MakeExecInOutNode("test.Exec");
    auto data_def = MakeDataNode("test.Data", PinType::Int, PinType::Int);
    uint64_t nExec = g.AddNode(exec_def);
    uint64_t nData = g.AddNode(data_def);

    uint64_t data_out = MakePinId(nData, 0); // Data Output
    uint64_t exec_in  = MakePinId(nExec, 0); // Exec Input
    REQUIRE(g.Connect(data_out, exec_in) == 0);
}

TEST_CASE("Bool-to-Int data connection rejected", "[graph][connections]") {
    ScriptGraph g;
    auto bool_def = MakeDataNode("test.Bool", PinType::Bool, PinType::Bool);
    auto int_def  = MakeDataNode("test.Int",  PinType::Int,  PinType::Int);
    uint64_t nBool = g.AddNode(bool_def);
    uint64_t nInt  = g.AddNode(int_def);

    uint64_t bool_out = MakePinId(nBool, 0);
    uint64_t int_in   = MakePinId(nInt, 1);
    REQUIRE(g.Connect(bool_out, int_in) == 0);
}

TEST_CASE("Actor-to-ObjectRef connection accepted", "[graph][connections]") {
    ScriptGraph g;
    auto actor_def  = MakeDataNode("test.Actor",    PinType::Actor,     PinType::Actor);
    auto objref_def = MakeDataNode("test.ObjRef",   PinType::ObjectRef, PinType::ObjectRef);
    uint64_t nActor  = g.AddNode(actor_def);
    uint64_t nObjRef = g.AddNode(objref_def);

    uint64_t actor_out  = MakePinId(nActor,  0); // Actor Output
    uint64_t objref_in  = MakePinId(nObjRef, 1); // ObjectRef Input
    REQUIRE(g.Connect(actor_out, objref_in) != 0);
}

TEST_CASE("Data input pin: second connection replaces first", "[graph][connections]") {
    ScriptGraph g;
    auto src1 = MakeDataNode("test.S1", PinType::Int, PinType::Int);
    auto src2 = MakeDataNode("test.S2", PinType::Int, PinType::Int);
    auto dst  = MakeDataNode("test.Dst", PinType::Int, PinType::Int);
    uint64_t nS1  = g.AddNode(src1);
    uint64_t nS2  = g.AddNode(src2);
    uint64_t nDst = g.AddNode(dst);

    uint64_t s1_out  = MakePinId(nS1,  0);
    uint64_t s2_out  = MakePinId(nS2,  0);
    uint64_t dst_in  = MakePinId(nDst, 1);

    REQUIRE(g.Connect(s1_out, dst_in) != 0);
    REQUIRE(g.connections.size() == 1);

    REQUIRE(g.Connect(s2_out, dst_in) != 0); // replaces the first
    REQUIRE(g.connections.size() == 1);
    REQUIRE(g.connections[0].from_pin_id == s2_out);
}

TEST_CASE("Exec output pin can fan-out to multiple inputs", "[graph][connections]") {
    ScriptGraph g;
    auto src  = MakeExecNode("test.ExecSrc");
    auto dstA = MakeExecInOutNode("test.DstA");
    auto dstB = MakeExecInOutNode("test.DstB");
    uint64_t nSrc  = g.AddNode(src);
    uint64_t nDstA = g.AddNode(dstA);
    uint64_t nDstB = g.AddNode(dstB);

    uint64_t exec_out  = MakePinId(nSrc,  0);
    uint64_t dstA_in   = MakePinId(nDstA, 0);
    uint64_t dstB_in   = MakePinId(nDstB, 0);

    REQUIRE(g.Connect(exec_out, dstA_in) != 0);
    REQUIRE(g.Connect(exec_out, dstB_in) != 0);
    REQUIRE(g.connections.size() == 2);
}

TEST_CASE("Same-node connection rejected", "[graph][connections]") {
    ScriptGraph g;
    auto def = MakeDataNode("test.Self", PinType::Int, PinType::Int);
    uint64_t n = g.AddNode(def);
    uint64_t out = MakePinId(n, 0);
    uint64_t in  = MakePinId(n, 1);
    REQUIRE(g.Connect(out, in) == 0);
}

TEST_CASE("CanConnect agrees with Connect", "[graph][connections]") {
    ScriptGraph g;
    auto src = MakeDataNode("test.Src", PinType::Int, PinType::Int);
    auto dst = MakeDataNode("test.Dst", PinType::Int, PinType::Int);
    uint64_t nSrc = g.AddNode(src);
    uint64_t nDst = g.AddNode(dst);
    uint64_t src_out = MakePinId(nSrc, 0);
    uint64_t dst_in  = MakePinId(nDst, 1);

    REQUIRE(g.CanConnect(src_out, dst_in) == true);
    REQUIRE(g.Connect(src_out, dst_in) != 0);

    // Reversed direction: output → output should fail
    uint64_t src_in  = MakePinId(nSrc, 1);
    uint64_t dst_out = MakePinId(nDst, 0);
    REQUIRE(g.CanConnect(src_out, dst_out) == false);
    REQUIRE(g.CanConnect(src_in,  dst_in)  == false);
}
