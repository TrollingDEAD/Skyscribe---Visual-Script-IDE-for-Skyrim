#include "graph/ScriptGraph.h"

#include <algorithm>

namespace graph {

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
