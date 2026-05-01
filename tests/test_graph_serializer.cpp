#include <catch2/catch_test_macros.hpp>
#include "graph/GraphSerializer.h"
#include "graph/NodeRegistry.h"
#include "graph/BuiltinNodes.h"

using namespace graph;

// Ensure builtin nodes are registered once for the whole test run.
struct RegistryFixture {
    RegistryFixture() { BuiltinNodes::RegisterAll(); }
};
static RegistryFixture s_registry_fixture;

TEST_CASE("GraphSerializer round-trip with nodes and connection", "[serializer]") {
    ScriptGraph g;
    g.script_name = "TestScript";
    g.extends     = "ObjectReference";

    const NodeDefinition* def_init = NodeRegistry::Get().Find("builtin.OnInit");
    const NodeDefinition* def_notif = NodeRegistry::Get().Find("builtin.Notification");
    REQUIRE(def_init  != nullptr);
    REQUIRE(def_notif != nullptr);

    uint64_t n1 = g.AddNode(*def_init,  100.0f, 200.0f);
    uint64_t n2 = g.AddNode(*def_notif, 300.0f, 200.0f);

    // OnInit: pin 0 = ExecOut — Notification: pin 0 = ExecIn
    uint64_t conn_id = g.Connect(MakePinId(n1, 0), MakePinId(n2, 0));
    REQUIRE(conn_id != 0);

    auto j  = GraphSerializer::Save(g);
    auto g2 = GraphSerializer::Load(j);

    REQUIRE(g2.script_name  == g.script_name);
    REQUIRE(g2.extends      == g.extends);
    REQUIRE(g2.next_node_id == g.next_node_id);
    REQUIRE(g2.next_conn_id == g.next_conn_id);
    REQUIRE(g2.nodes.size()       == g.nodes.size());
    REQUIRE(g2.connections.size() == g.connections.size());
    REQUIRE(g2.nodes[0].id    == g.nodes[0].id);
    REQUIRE(g2.nodes[0].pos_x == g.nodes[0].pos_x);
    REQUIRE(g2.nodes[0].pos_y == g.nodes[0].pos_y);
    REQUIRE(g2.connections[0].from_pin_id == g.connections[0].from_pin_id);
    REQUIRE(g2.connections[0].to_pin_id   == g.connections[0].to_pin_id);
}

TEST_CASE("GraphSerializer skips unknown type_id", "[serializer]") {
    ScriptGraph g;
    g.script_name = "TestScript";

    const NodeDefinition* def = NodeRegistry::Get().Find("builtin.OnInit");
    REQUIRE(def != nullptr);
    g.AddNode(*def);

    auto j = GraphSerializer::Save(g);
    j["nodes"][0]["type_id"] = "nonexistent.FakeNode";

    auto g2 = GraphSerializer::Load(j);
    REQUIRE(g2.nodes.empty()); // skipped
}

TEST_CASE("GraphSerializer next_node_id preserved so IDs are never reused", "[serializer]") {
    ScriptGraph g;
    const NodeDefinition* def = NodeRegistry::Get().Find("builtin.OnInit");
    REQUIRE(def != nullptr);
    g.AddNode(*def);
    g.AddNode(*def);
    g.AddNode(*def);

    auto j  = GraphSerializer::Save(g);
    auto g2 = GraphSerializer::Load(j);
    REQUIRE(g2.next_node_id == g.next_node_id);

    // New node after reload must not reuse any old ID
    uint64_t new_id = g2.AddNode(*def);
    for (const auto& n : g.nodes)
        REQUIRE(new_id != n.id);
}

TEST_CASE("GraphSerializer empty graph round-trip", "[serializer]") {
    ScriptGraph g;
    g.script_name = "Empty";
    g.extends     = "Quest";

    auto j  = GraphSerializer::Save(g);
    auto g2 = GraphSerializer::Load(j);

    REQUIRE(g2.script_name == "Empty");
    REQUIRE(g2.extends     == "Quest");
    REQUIRE(g2.nodes.empty());
    REQUIRE(g2.connections.empty());
}

TEST_CASE("GraphSerializer pin value override survives round-trip", "[serializer]") {
    ScriptGraph g;
    const NodeDefinition* def = NodeRegistry::Get().Find("builtin.LiteralInt");
    REQUIRE(def != nullptr);
    uint64_t n = g.AddNode(*def);

    ScriptNode* node = g.FindNode(n);
    REQUIRE(node != nullptr);
    REQUIRE(!node->pins.empty());
    node->pins[0].value = "42";

    auto j  = GraphSerializer::Save(g);
    auto g2 = GraphSerializer::Load(j);

    REQUIRE(g2.nodes.size() == 1);
    REQUIRE(!g2.nodes[0].pins.empty());
    REQUIRE(g2.nodes[0].pins[0].value == "42");
}
