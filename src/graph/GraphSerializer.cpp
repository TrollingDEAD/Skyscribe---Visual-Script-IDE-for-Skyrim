#include "graph/GraphSerializer.h"
#include "graph/NodeRegistry.h"

#include <unordered_map>

namespace graph {

// â”€â”€ helpers: PinType â†” string â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static std::string PinTypeToString(PinType t) {
    switch (t) {
        case PinType::Exec:          return "Exec";
        case PinType::Bool:          return "Bool";
        case PinType::Int:           return "Int";
        case PinType::Float:         return "Float";
        case PinType::String:        return "String";
        case PinType::ObjectRef:     return "ObjectRef";
        case PinType::Actor:         return "Actor";
        case PinType::Quest:         return "Quest";
        case PinType::Form:          return "Form";
        case PinType::Array_Bool:    return "Array_Bool";
        case PinType::Array_Int:     return "Array_Int";
        case PinType::Array_Float:   return "Array_Float";
        case PinType::Array_String:  return "Array_String";
        case PinType::Array_ObjectRef: return "Array_ObjectRef";
        default:                     return "Unknown";
    }
}

static PinType PinTypeFromString(const std::string& s) {
    if (s == "Exec")          return PinType::Exec;
    if (s == "Bool")          return PinType::Bool;
    if (s == "Int")           return PinType::Int;
    if (s == "Float")         return PinType::Float;
    if (s == "String")        return PinType::String;
    if (s == "ObjectRef")     return PinType::ObjectRef;
    if (s == "Actor")         return PinType::Actor;
    if (s == "Quest")         return PinType::Quest;
    if (s == "Form")          return PinType::Form;
    if (s == "Array_Bool")    return PinType::Array_Bool;
    if (s == "Array_Int")     return PinType::Array_Int;
    if (s == "Array_Float")   return PinType::Array_Float;
    if (s == "Array_String")  return PinType::Array_String;
    if (s == "Array_ObjectRef") return PinType::Array_ObjectRef;
    return PinType::Unknown;
}

// â”€â”€ SaveGraph / LoadGraph (internal, forward-declared) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static nlohmann::json SaveGraph(const ScriptGraph& g);
static ScriptGraph    LoadGraph(const nlohmann::json& j);

// â”€â”€ Save â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

nlohmann::json GraphSerializer::Save(const ScriptGraph& g) { return SaveGraph(g); }

static nlohmann::json SaveGraph(const ScriptGraph& g) {
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

    // Properties
    auto& jprops = j["properties"] = nlohmann::json::array();
    for (const auto& prop : g.properties) {
        nlohmann::json jp;
        jp["name"]          = prop.name;
        jp["type"]          = PinTypeToString(prop.type);
        jp["kind"]          = static_cast<int>(prop.kind);
        jp["default_value"] = prop.default_value;
        jp["tooltip"]       = prop.tooltip;
        jprops.push_back(std::move(jp));
    }

    // Functions
    auto& jfuncs = j["functions"] = nlohmann::json::array();
    for (const auto& func : g.functions) {
        nlohmann::json jf;
        jf["name"]        = func.name;
        jf["return_type"] = PinTypeToString(func.return_type);
        jf["is_global"]   = func.is_global;

        auto& jparams = jf["parameters"] = nlohmann::json::array();
        for (const auto& param : func.parameters) {
            nlohmann::json jp;
            jp["name"]          = param.name;
            jp["type"]          = PinTypeToString(param.type);
            jp["default_value"] = param.default_value;
            jp["tooltip"]       = param.tooltip;
            jparams.push_back(std::move(jp));
        }

        jf["body_graph"] = SaveGraph(*func.body_graph);
        jfuncs.push_back(std::move(jf));
    }

    return j;
}

// â”€â”€ Load â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

ScriptGraph GraphSerializer::Load(const nlohmann::json& j) { return LoadGraph(j); }

static ScriptGraph LoadGraph(const nlohmann::json& j) {
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
        if (!def) continue; // unknown type â€” forward-compat: skip silently

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

    // Properties
    for (const auto& jp : j.value("properties", nlohmann::json::array())) {
        PropertyDefinition prop;
        prop.name          = jp.value("name",          std::string{});
        prop.type          = PinTypeFromString(jp.value("type", std::string{"Unknown"}));
        prop.kind          = static_cast<PropertyKind>(jp.value("kind", 0));
        prop.default_value = jp.value("default_value", std::string{});
        prop.tooltip       = jp.value("tooltip",       std::string{});
        g.properties.push_back(std::move(prop));
    }

    // Functions â€” must register nodes in NodeRegistry before loading body graphs
    for (const auto& jf : j.value("functions", nlohmann::json::array())) {
        FunctionDefinition func;
        func.name        = jf.value("name",        std::string{});
        func.return_type = PinTypeFromString(jf.value("return_type", std::string{"Unknown"}));
        func.is_global   = jf.value("is_global",   false);

        for (const auto& jp : jf.value("parameters", nlohmann::json::array())) {
            PinDefinition param;
            param.name          = jp.value("name",          std::string{});
            param.type          = PinTypeFromString(jp.value("type", std::string{"Unknown"}));
            param.default_value = jp.value("default_value", std::string{});
            param.tooltip       = jp.value("tooltip",       std::string{});
            param.kind          = PinKind::Input;
            param.flow          = PinFlow::Data;
            func.parameters.push_back(std::move(param));
        }

        // Register dynamic nodes so body_graph nodes can be resolved
        const std::string& sn = g.script_name;
        NodeRegistry::Get().Register(
            [&]() {
                NodeDefinition d;
                d.type_id      = "script." + sn + ".entry." + func.name;
                d.display_name = func.name + "  [ENTRY]";
                d.category     = NodeCategory::Event;
                PinDefinition exec_out;
                exec_out.name = "Out"; exec_out.kind = PinKind::Output;
                exec_out.flow = PinFlow::Execution; exec_out.type = PinType::Exec;
                d.pins.push_back(exec_out);
                for (const auto& param : func.parameters) {
                    PinDefinition p = param;
                    p.kind = PinKind::Output; p.flow = PinFlow::Data;
                    d.pins.push_back(p);
                }
                return d;
            }()
        );
        NodeRegistry::Get().Register(
            [&]() {
                NodeDefinition d;
                d.type_id      = "script." + sn + ".return." + func.name;
                d.display_name = "Return  [RETURN]";
                d.category     = NodeCategory::ControlFlow;
                d.codegen_template = (func.return_type != PinType::Unknown) ? "Return {Value}" : "Return";
                PinDefinition exec_in;
                exec_in.name = "In"; exec_in.kind = PinKind::Input;
                exec_in.flow = PinFlow::Execution; exec_in.type = PinType::Exec;
                d.pins.push_back(exec_in);
                if (func.return_type != PinType::Unknown) {
                    PinDefinition val;
                    val.name = "Value"; val.kind = PinKind::Input;
                    val.flow = PinFlow::Data; val.type = func.return_type;
                    d.pins.push_back(val);
                }
                return d;
            }()
        );
        {
            NodeDefinition d;
            d.type_id      = "script." + sn + ".call." + func.name;
            d.display_name = "Call " + func.name;
            d.category     = NodeCategory::Custom;
            std::string tmpl = func.name + "(";
            for (size_t i = 0; i < func.parameters.size(); ++i) {
                if (i) tmpl += ", ";
                tmpl += "{" + func.parameters[i].name + "}";
            }
            tmpl += ")";
            d.codegen_template = tmpl;
            PinDefinition exec_in;
            exec_in.name = "In"; exec_in.kind = PinKind::Input;
            exec_in.flow = PinFlow::Execution; exec_in.type = PinType::Exec;
            d.pins.push_back(exec_in);
            for (const auto& param : func.parameters) {
                PinDefinition p = param;
                p.kind = PinKind::Input; p.flow = PinFlow::Data;
                d.pins.push_back(p);
            }
            PinDefinition exec_out;
            exec_out.name = "Out"; exec_out.kind = PinKind::Output;
            exec_out.flow = PinFlow::Execution; exec_out.type = PinType::Exec;
            d.pins.push_back(exec_out);
            if (func.return_type != PinType::Unknown) {
                PinDefinition ret;
                ret.name = "ReturnValue"; ret.kind = PinKind::Output;
                ret.flow = PinFlow::Data; ret.type = func.return_type;
                d.pins.push_back(ret);
            }
            NodeRegistry::Get().Register(std::move(d));
        }

        // Load body graph
        if (jf.contains("body_graph"))
            *func.body_graph = LoadGraph(jf["body_graph"]);

        g.functions.push_back(std::move(func));
    }

    return g;
}

} // namespace graph

