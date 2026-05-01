#include "graph/GraphSerializer.h"
#include "graph/NodeRegistry.h"

#include <unordered_map>

namespace graph {

nlohmann::json GraphSerializer::Save(const ScriptGraph& g) {
    nlohmann::json j;
    j["script_name"]  = g.script_name;
    j["extends"]      = g.extends;
    j["next_node_id"] = g.next_node_id;
    j["next_conn_id"] = g.next_conn_id;

    auto& jnodes = j["nodes"] = nlohmann::json::array();
    for (const auto& n : g.nodes) {
        nlohmann::json jn;
        jn["id"]      = n.id;
        jn["type_id"] = n.type_id;
        jn["x"]       = n.pos_x;
        jn["y"]       = n.pos_y;

        // Only serialize non-empty pin value overrides to keep JSON compact.
        auto& jpins = jn["pins"] = nlohmann::json::array();
        for (const auto& p : n.pins) {
            if (!p.value.empty()) {
                nlohmann::json jp;
                jp["id"]    = p.id;
                jp["value"] = p.value;
                jpins.push_back(std::move(jp));
            }
        }
        jnodes.push_back(std::move(jn));
    }

    auto& jconns = j["connections"] = nlohmann::json::array();
    for (const auto& c : g.connections) {
        nlohmann::json jc;
        jc["id"]   = c.id;
        jc["from"] = c.from_pin_id;
        jc["to"]   = c.to_pin_id;
        jconns.push_back(std::move(jc));
    }
    return j;
}

ScriptGraph GraphSerializer::Load(const nlohmann::json& j) {
    ScriptGraph g;
    g.script_name  = j.value("script_name",  std::string{});
    g.extends      = j.value("extends",      std::string{});
    g.next_node_id = j.value("next_node_id", uint64_t{1});
    g.next_conn_id = j.value("next_conn_id", uint64_t{1});

    for (const auto& jn : j.value("nodes", nlohmann::json::array())) {
        uint64_t    node_id = jn.value("id",      uint64_t{0});
        std::string type_id = jn.value("type_id", std::string{});
        float       x       = jn.value("x",       0.0f);
        float       y       = jn.value("y",       0.0f);

        const NodeDefinition* def = NodeRegistry::Get().Find(type_id);
        if (!def) continue; // unknown type — forward-compat: skip silently

        // Build override map from saved pins (only non-defaults are stored)
        std::unordered_map<uint64_t, std::string> overrides;
        for (const auto& jp : jn.value("pins", nlohmann::json::array()))
            overrides[jp.value("id", uint64_t{0})] = jp.value("value", std::string{});

        ScriptNode n;
        n.id      = node_id;
        n.type_id = type_id;
        n.pos_x   = x;
        n.pos_y   = y;

        for (uint32_t i = 0; i < static_cast<uint32_t>(def->pins.size()); ++i) {
            const auto& pd = def->pins[i];
            Pin p;
            p.id    = MakePinId(node_id, i);
            p.name  = pd.name;
            p.kind  = pd.kind;
            p.flow  = pd.flow;
            p.type  = pd.type;
            p.value = pd.default_value;
            auto it = overrides.find(p.id);
            if (it != overrides.end()) p.value = it->second;
            n.pins.push_back(std::move(p));
        }
        g.nodes.push_back(std::move(n));
    }

    for (const auto& jc : j.value("connections", nlohmann::json::array())) {
        Connection c;
        c.id          = jc.value("id",   uint64_t{0});
        c.from_pin_id = jc.value("from", uint64_t{0});
        c.to_pin_id   = jc.value("to",   uint64_t{0});
        g.connections.push_back(std::move(c));
    }
    return g;
}

} // namespace graph
