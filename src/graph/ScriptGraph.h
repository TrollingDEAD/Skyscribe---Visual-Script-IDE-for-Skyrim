#pragma once

#include "graph/NodeDefinition.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace graph {

// ── Runtime instances ─────────────────────────────────────────────────────────
//
// Pin IDs are computed deterministically:
//   pin_id = (node_id << 16) | pin_index_within_node
// This means no separate counter is needed and pin IDs are stable.

struct Pin {
    uint64_t    id    = 0;
    std::string name;
    PinKind     kind  = PinKind::Input;
    PinFlow     flow  = PinFlow::Data;
    PinType     type  = PinType::Unknown;
    std::string value;  // default / literal value override
};

struct ScriptNode {
    uint64_t          id      = 0;
    std::string       type_id; // references NodeDefinition::type_id
    float             pos_x   = 0.0f;
    float             pos_y   = 0.0f;
    std::vector<Pin>  pins;
};

struct Connection {
    uint64_t id          = 0;
    uint64_t from_pin_id = 0; // Output pin
    uint64_t to_pin_id   = 0; // Input pin
};

// ── Graph ─────────────────────────────────────────────────────────────────────

struct ScriptGraph {
    std::string              script_name;
    std::string              extends;
    std::vector<ScriptNode>  nodes;
    std::vector<Connection>  connections;

    uint64_t next_node_id = 1; // monotonically increasing; never reused
    uint64_t next_conn_id = 1;

    // ── Mutation helpers ──────────────────────────────────────────────────────

    // Add a node built from a definition. Returns the new node's id.
    uint64_t AddNode(const NodeDefinition& def, float x = 0.0f, float y = 0.0f);

    // Remove a node and all its connections. No-op if id not found.
    void RemoveNode(uint64_t node_id);

    // Create a connection from an Output pin to an Input pin.
    // Enforces flow/type compatibility. Returns the new connection id,
    // or 0 if the connection was rejected.
    uint64_t Connect(uint64_t from_pin_id, uint64_t to_pin_id);

    // Remove a connection by id. No-op if not found.
    void Disconnect(uint64_t conn_id);

    // Remove all connections touching a specific pin.
    void DisconnectPin(uint64_t pin_id);

    // Read-only queries ────────────────────────────────────────────────────────

    // Returns true if this wire would be accepted by Connect().
    bool CanConnect(uint64_t from_pin_id, uint64_t to_pin_id) const;

    ScriptNode*       FindNode(uint64_t node_id);
    const ScriptNode* FindNode(uint64_t node_id) const;

    Pin*       FindPin(uint64_t pin_id);
    const Pin* FindPin(uint64_t pin_id) const;

    // Returns the node that owns the given pin, or nullptr.
    ScriptNode*       OwnerNode(uint64_t pin_id);
    const ScriptNode* OwnerNode(uint64_t pin_id) const;

    // Returns all connections that touch a given pin.
    std::vector<const Connection*> ConnectionsForPin(uint64_t pin_id) const;
};

// ── Pin ID encoding ───────────────────────────────────────────────────────────

inline uint64_t MakePinId(uint64_t node_id, uint32_t pin_index) {
    return (node_id << 16) | static_cast<uint64_t>(pin_index);
}

inline uint64_t PinOwnerNodeId(uint64_t pin_id) {
    return pin_id >> 16;
}

} // namespace graph
