#include "graph/ScriptGraph.h"
#include "graph/NodeRegistry.h"

#include <algorithm>

namespace graph {

// ── FunctionDefinition — rule-of-five (ScriptGraph must be complete here) ────

FunctionDefinition::FunctionDefinition()
    : body_graph(std::make_unique<ScriptGraph>()) {}

FunctionDefinition::~FunctionDefinition() = default;

FunctionDefinition::FunctionDefinition(FunctionDefinition&&) noexcept = default;
FunctionDefinition& FunctionDefinition::operator=(FunctionDefinition&&) noexcept = default;

FunctionDefinition::FunctionDefinition(const FunctionDefinition& other)
    : name(other.name)
    , return_type(other.return_type)
    , parameters(other.parameters)
    , is_global(other.is_global)
    , body_graph(other.body_graph
          ? std::make_unique<ScriptGraph>(*other.body_graph)
          : std::make_unique<ScriptGraph>())
{}

FunctionDefinition& FunctionDefinition::operator=(const FunctionDefinition& other) {
    if (this != &other) {
        name        = other.name;
        return_type = other.return_type;
        parameters  = other.parameters;
        is_global   = other.is_global;
        body_graph  = other.body_graph
            ? std::make_unique<ScriptGraph>(*other.body_graph)
            : std::make_unique<ScriptGraph>();
    }
    return *this;
}

// ── Function helpers ──────────────────────────────────────────────────────────

static NodeDefinition MakeFunctionEntryDef(const std::string& script_name,
                                            const std::string& func_name,
                                            const std::vector<PinDefinition>& params) {
    NodeDefinition d;
    d.type_id      = "script." + script_name + ".entry." + func_name;
    d.display_name = func_name + "  [ENTRY]";
    d.category     = NodeCategory::Event;
    // One exec-out + one data-out per parameter
    PinDefinition exec_out;
    exec_out.name = "Out"; exec_out.kind = PinKind::Output;
    exec_out.flow = PinFlow::Execution; exec_out.type = PinType::Exec;
    d.pins.push_back(exec_out);
    for (const auto& param : params) {
        PinDefinition p = param;
        p.kind = PinKind::Output;
        p.flow = PinFlow::Data;
        d.pins.push_back(p);
    }
    return d;
}

static NodeDefinition MakeFunctionReturnDef(const std::string& script_name,
                                             const std::string& func_name,
                                             PinType return_type) {
    NodeDefinition d;
    d.type_id      = "script." + script_name + ".return." + func_name;
    d.display_name = "Return  [RETURN]";
    d.category     = NodeCategory::ControlFlow;
    d.codegen_template = (return_type != PinType::Unknown) ? "Return {Value}" : "Return";
    PinDefinition exec_in;
    exec_in.name = "In"; exec_in.kind = PinKind::Input;
    exec_in.flow = PinFlow::Execution; exec_in.type = PinType::Exec;
    d.pins.push_back(exec_in);
    if (return_type != PinType::Unknown) {
        PinDefinition val;
        val.name = "Value"; val.kind = PinKind::Input;
        val.flow = PinFlow::Data; val.type = return_type;
        d.pins.push_back(val);
    }
    return d;
}

static NodeDefinition MakeFunctionCallDef(const std::string& script_name,
                                           const std::string& func_name,
                                           PinType return_type,
                                           const std::vector<PinDefinition>& params) {
    NodeDefinition d;
    d.type_id      = "script." + script_name + ".call." + func_name;
    d.display_name = "Call " + func_name;
    d.category     = NodeCategory::Custom;

    // Build codegen template: "FuncName({Param0}, {Param1}, ...)"
    std::string tmpl = func_name + "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i) tmpl += ", ";
        tmpl += "{" + params[i].name + "}";
    }
    tmpl += ")";
    d.codegen_template = tmpl;

    PinDefinition exec_in;
    exec_in.name = "In"; exec_in.kind = PinKind::Input;
    exec_in.flow = PinFlow::Execution; exec_in.type = PinType::Exec;
    d.pins.push_back(exec_in);
    for (const auto& param : params) {
        PinDefinition p = param;
        p.kind = PinKind::Input;
        p.flow = PinFlow::Data;
        d.pins.push_back(p);
    }
    PinDefinition exec_out;
    exec_out.name = "Out"; exec_out.kind = PinKind::Output;
    exec_out.flow = PinFlow::Execution; exec_out.type = PinType::Exec;
    d.pins.push_back(exec_out);
    if (return_type != PinType::Unknown) {
        PinDefinition ret;
        ret.name = "ReturnValue"; ret.kind = PinKind::Output;
        ret.flow = PinFlow::Data; ret.type = return_type;
        d.pins.push_back(ret);
    }
    return d;
}

FunctionDefinition& ScriptGraph::AddFunction(const std::string& name,
                                               PinType return_type,
                                               const std::vector<PinDefinition>& params,
                                               bool is_global) {
    FunctionDefinition func;
    func.name        = name;
    func.return_type = return_type;
    func.parameters  = params;
    func.is_global   = is_global;

    // Register dynamic nodes in NodeRegistry
    NodeRegistry::Get().Register(MakeFunctionEntryDef(script_name, name, params));
    NodeRegistry::Get().Register(MakeFunctionReturnDef(script_name, name, return_type));
    NodeRegistry::Get().Register(MakeFunctionCallDef(script_name, name, return_type, params));

    // Auto-place Entry node in the body graph
    auto entry_def = NodeRegistry::Get().Find("script." + script_name + ".entry." + name);
    if (entry_def)
        func.body_graph->AddNode(*entry_def, 80.0f, 80.0f);

    functions.push_back(std::move(func));
    return functions.back();
}

void ScriptGraph::RemoveFunction(const std::string& name) {
    auto it = std::find_if(functions.begin(), functions.end(),
        [&name](const FunctionDefinition& f) { return f.name == name; });
    if (it == functions.end()) return;

    NodeRegistry::Get().Unregister("script." + script_name + ".entry."  + name);
    NodeRegistry::Get().Unregister("script." + script_name + ".return." + name);
    NodeRegistry::Get().Unregister("script." + script_name + ".call."   + name);

    functions.erase(it);
}

void ScriptGraph::RenameFunction(const std::string& old_name, const std::string& new_name) {
    auto it = std::find_if(functions.begin(), functions.end(),
        [&old_name](const FunctionDefinition& f) { return f.name == old_name; });
    if (it == functions.end()) return;

    // Remove old registry entries
    NodeRegistry::Get().Unregister("script." + script_name + ".entry."  + old_name);
    NodeRegistry::Get().Unregister("script." + script_name + ".return." + old_name);
    NodeRegistry::Get().Unregister("script." + script_name + ".call."   + old_name);

    it->name = new_name;

    // Re-register with new name
    NodeRegistry::Get().Register(MakeFunctionEntryDef(script_name, new_name, it->parameters));
    NodeRegistry::Get().Register(MakeFunctionReturnDef(script_name, new_name, it->return_type));
    NodeRegistry::Get().Register(MakeFunctionCallDef(script_name, new_name, it->return_type, it->parameters));
}

FunctionDefinition* ScriptGraph::FindFunction(const std::string& name) {
    for (auto& f : functions)
        if (f.name == name) return &f;
    return nullptr;
}

const FunctionDefinition* ScriptGraph::FindFunction(const std::string& name) const {
    for (const auto& f : functions)
        if (f.name == name) return &f;
    return nullptr;
}

uint64_t ScriptGraph::AddNode(const NodeDefinition& def, float x, float y) {
    ScriptNode node;
    node.id      = next_node_id++;
    node.type_id = def.type_id;
    node.pos_x   = x;
    node.pos_y   = y;

    for (uint32_t i = 0; i < static_cast<uint32_t>(def.pins.size()); ++i) {
        const auto& pd = def.pins[i];
        Pin p;
        p.id    = MakePinId(node.id, i);
        p.name  = pd.name;
        p.kind  = pd.kind;
        p.flow  = pd.flow;
        p.type  = pd.type;
        p.value = pd.default_value;
        node.pins.push_back(std::move(p));
    }

    uint64_t id = node.id;
    nodes.push_back(std::move(node));
    return id;
}

void ScriptGraph::RemoveNode(uint64_t node_id) {
    auto it = std::find_if(nodes.begin(), nodes.end(),
        [node_id](const ScriptNode& n) { return n.id == node_id; });
    if (it == nodes.end()) return;

    // Remove all connections touching any pin of this node.
    for (const auto& pin : it->pins)
        DisconnectPin(pin.id);

    nodes.erase(it);
}

uint64_t ScriptGraph::Connect(uint64_t from_pin_id, uint64_t to_pin_id) {
    if (!CanConnect(from_pin_id, to_pin_id)) return 0;

    const Pin* to_pin = FindPin(to_pin_id);

    // Data input pins are single-consumer: replace any existing connection.
    if (to_pin && to_pin->flow == PinFlow::Data && to_pin->kind == PinKind::Input)
        DisconnectPin(to_pin_id);

    Connection conn;
    conn.id          = next_conn_id++;
    conn.from_pin_id = from_pin_id;
    conn.to_pin_id   = to_pin_id;
    uint64_t id = conn.id;
    connections.push_back(std::move(conn));
    return id;
}

void ScriptGraph::Disconnect(uint64_t conn_id) {
    auto it = std::find_if(connections.begin(), connections.end(),
        [conn_id](const Connection& c) { return c.id == conn_id; });
    if (it != connections.end())
        connections.erase(it);
}

void ScriptGraph::DisconnectPin(uint64_t pin_id) {
    connections.erase(
        std::remove_if(connections.begin(), connections.end(),
            [pin_id](const Connection& c) {
                return c.from_pin_id == pin_id || c.to_pin_id == pin_id;
            }),
        connections.end());
}

bool ScriptGraph::CanConnect(uint64_t from_pin_id, uint64_t to_pin_id) const {
    const Pin* from = FindPin(from_pin_id);
    const Pin* to   = FindPin(to_pin_id);

    if (!from || !to) return false;
    if (from->kind != PinKind::Output) return false;
    if (to->kind   != PinKind::Input)  return false;
    if (from->flow != to->flow)        return false;
    if (from->flow == PinFlow::Data && !IsCompatible(from->type, to->type)) return false;
    if (PinOwnerNodeId(from_pin_id) == PinOwnerNodeId(to_pin_id)) return false;
    return true;
}

ScriptNode* ScriptGraph::FindNode(uint64_t node_id) {
    for (auto& n : nodes)
        if (n.id == node_id) return &n;
    return nullptr;
}

const ScriptNode* ScriptGraph::FindNode(uint64_t node_id) const {
    for (const auto& n : nodes)
        if (n.id == node_id) return &n;
    return nullptr;
}

Pin* ScriptGraph::FindPin(uint64_t pin_id) {
    uint64_t node_id = PinOwnerNodeId(pin_id);
    ScriptNode* node = FindNode(node_id);
    if (!node) return nullptr;
    for (auto& p : node->pins)
        if (p.id == pin_id) return &p;
    return nullptr;
}

const Pin* ScriptGraph::FindPin(uint64_t pin_id) const {
    uint64_t node_id = PinOwnerNodeId(pin_id);
    const ScriptNode* node = FindNode(node_id);
    if (!node) return nullptr;
    for (const auto& p : node->pins)
        if (p.id == pin_id) return &p;
    return nullptr;
}

ScriptNode* ScriptGraph::OwnerNode(uint64_t pin_id) {
    return FindNode(PinOwnerNodeId(pin_id));
}

const ScriptNode* ScriptGraph::OwnerNode(uint64_t pin_id) const {
    return FindNode(PinOwnerNodeId(pin_id));
}

std::vector<const Connection*> ScriptGraph::ConnectionsForPin(uint64_t pin_id) const {
    std::vector<const Connection*> result;
    for (const auto& c : connections)
        if (c.from_pin_id == pin_id || c.to_pin_id == pin_id)
            result.push_back(&c);
    return result;
}

} // namespace graph
