#pragma once

#include "graph/NodeDefinition.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace graph {

// ── Property definition (task 3.11) ──────────────────────────────────────────

enum class PropertyKind { Auto, AutoReadOnly, Conditional };

struct PropertyDefinition {
    std::string  name;
    PinType      type          = PinType::Unknown;
    PropertyKind kind          = PropertyKind::Auto;
    std::string  default_value;
    std::string  tooltip;
};

// ── Function definition (task 3.9) ────────────────────────────────────────────
//
// body_graph is heap-allocated to break the recursive struct definition cycle
// (ScriptGraph owns FunctionDefinition which owns ScriptGraph).

struct ScriptGraph; // forward declaration

struct FunctionDefinition {
    std::string                name;
    PinType                    return_type = PinType::Unknown; // Unknown = None (void)
    std::vector<PinDefinition> parameters;
    bool                       is_global   = false;
    std::unique_ptr<ScriptGraph> body_graph;

    // Destructor / move defined in .cpp (ScriptGraph must be complete there).
    FunctionDefinition();
    ~FunctionDefinition();
    FunctionDefinition(FunctionDefinition&&) noexcept;
    FunctionDefinition& operator=(FunctionDefinition&&) noexcept;
    // Deep copy
    FunctionDefinition(const FunctionDefinition&);
    FunctionDefinition& operator=(const FunctionDefinition&);
};

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
    std::string                     script_name;
    std::string                     extends;
    std::vector<ScriptNode>         nodes;
    std::vector<Connection>         connections;
    std::vector<PropertyDefinition> properties;
    std::vector<FunctionDefinition> functions;

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

    // ── Function helpers ──────────────────────────────────────────────────────

    // Add a function. Places a FunctionEntry node in the body_graph.
    // Returns a reference to the new definition (valid until next mutation).
    FunctionDefinition& AddFunction(const std::string& name,
                                     PinType return_type = PinType::Unknown,
                                     const std::vector<PinDefinition>& params = {},
                                     bool is_global = false);

    // Remove a function by name. No-op if not found.
    void RemoveFunction(const std::string& name);

    // Rename a function. No-op if old_name not found.
    void RenameFunction(const std::string& old_name, const std::string& new_name);

    // Returns pointer to the function with this name, or nullptr.
    FunctionDefinition*       FindFunction(const std::string& name);
    const FunctionDefinition* FindFunction(const std::string& name) const;

    // ── Property helpers ──────────────────────────────────────────────────────

    // Add a property. Registers Get/Set nodes in NodeRegistry.
    // Returns reference valid until next mutation.
    PropertyDefinition& AddProperty(const std::string& name,
                                    PinType type,
                                    PropertyKind kind = PropertyKind::Auto,
                                    const std::string& default_value = "");

    // Remove a property by name. Unregisters Get/Set nodes.
    void RemoveProperty(const std::string& name);

    // Rename a property. Re-registers Get/Set nodes.
    void RenameProperty(const std::string& old_name, const std::string& new_name);

    // Returns pointer to the property with this name, or nullptr.
    PropertyDefinition*       FindProperty(const std::string& name);
    const PropertyDefinition* FindProperty(const std::string& name) const;

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
