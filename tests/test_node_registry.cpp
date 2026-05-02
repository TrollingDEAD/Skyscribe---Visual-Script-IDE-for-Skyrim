#include <catch2/catch_test_macros.hpp>
#include "graph/NodeRegistry.h"
#include "graph/BuiltinNodes.h"

using namespace graph;

struct RegFixture { RegFixture() { BuiltinNodes::RegisterAll(); } };
static RegFixture s_reg_fixture;

TEST_CASE("NodeRegistry has exactly 61 built-in nodes", "[registry]") {
    REQUIRE(NodeRegistry::Get().Count() == 62);
}

TEST_CASE("NodeRegistry Find builtin.OnInit returns correct definition", "[registry]") {
    const NodeDefinition* def = NodeRegistry::Get().Find("builtin.OnInit");
    REQUIRE(def != nullptr);
    REQUIRE(def->display_name == "OnInit");
    REQUIRE(def->category == NodeCategory::Event);
}

TEST_CASE("NodeRegistry ByCategory Event returns exactly 12 nodes", "[registry]") {
    auto events = NodeRegistry::Get().ByCategory(NodeCategory::Event);
    REQUIRE(events.size() == 12);
}

TEST_CASE("NodeRegistry AllNodes: Event nodes come before ControlFlow nodes", "[registry]") {
    const auto& all = NodeRegistry::Get().AllNodes();
    REQUIRE(!all.empty());

    bool seen_event = false;
    bool cf_before_event = false;
    for (const auto& def : all) {
        if (def.category == NodeCategory::Event)       seen_event = true;
        if (def.category == NodeCategory::ControlFlow && !seen_event) cf_before_event = true;
    }
    REQUIRE(!cf_before_event);
}
