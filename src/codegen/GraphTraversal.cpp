#include "codegen/GraphTraversal.h"
#include "graph/NodeRegistry.h"

#include <functional>
#include <string>
#include <unordered_set>

namespace codegen {

// Follow the exec-flow connection out of an exec-OUT pin to the next node
// with an exec-IN pin. Returns that node id, or 0 if not connected.
static uint64_t NextExecNode(const graph::ScriptGraph& g, uint64_t exec_out_pin_id) {
    for (const auto& conn : g.connections) {
        if (conn.from_pin_id == exec_out_pin_id) {
            // to_pin_id belongs to the next node
            return graph::PinOwnerNodeId(conn.to_pin_id);
        }
    }
    return 0;
}

// Follow an exec-OUT pin and collect every reachable next node (usually 0 or 1).
static std::vector<uint64_t> NextExecNodes(const graph::ScriptGraph& g,
                                           uint64_t exec_out_pin_id) {
    std::vector<uint64_t> out;
    for (const auto& conn : g.connections) {
        if (conn.from_pin_id == exec_out_pin_id)
            out.push_back(graph::PinOwnerNodeId(conn.to_pin_id));
    }
    return out;
}

// Returns the first exec-OUT pin id of a node whose output is an exec pin
// (used to start traversal from an Event node).
static uint64_t FirstExecOutPin(const graph::ScriptNode& node) {
    for (const auto& p : node.pins) {
        if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output)
            return p.id;
    }
    return 0;
}

static std::string DefaultLiteralForType(graph::PinType t) {
    using graph::PinType;
    switch (t) {
    case PinType::Bool:  return "False";
    case PinType::Int:   return "0";
    case PinType::Float: return "0.0";
    case PinType::String:return "\"\"";
    case PinType::Actor:
    case PinType::ObjectRef:
    case PinType::Form:
    case PinType::Quest:
    case PinType::Array_Bool:
    case PinType::Array_Int:
    case PinType::Array_Float:
    case PinType::Array_String:
    case PinType::Array_ObjectRef:
    case PinType::Unknown:
    default:             return "None";
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

TraversalResult GraphTraversal::Traverse(const graph::ScriptGraph& g,
                                          uint64_t start_exec_out_pin_id) {
    TraversalResult result;
    std::unordered_set<uint64_t> visited;

    uint64_t next_node = NextExecNode(g, start_exec_out_pin_id);

    while (next_node != 0) {
        if (visited.count(next_node)) {
            // Cycle detected
            result.has_cycle = true;
            // Find the last node we came from to record the edge
            uint64_t prev = result.node_ids.empty() ? 0 : result.node_ids.back();
            result.cycle_edges.push_back({prev, next_node});
            break;
        }
        visited.insert(next_node);
        result.node_ids.push_back(next_node);

        // Find the "primary" exec-out pin to continue the chain.
        // For most nodes this is the first exec-out pin named "Out".
        // For If/Else and While, we stop here — the string builder handles their branches.
        const graph::ScriptNode* node = g.FindNode(next_node);
        if (!node) break;

        // Look up the node definition to detect special control-flow types.
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node->type_id);
        if (!def) break;

        // IfElse and Sequence have branching exec-outs but no single "after" pin.
        // Stop the linear chain here; the string builder handles the branches.
        if (node->type_id == "builtin.IfElse" ||
            node->type_id == "builtin.Sequence") {
            break;
        }

        // WhileLoop: the "Loop Body" is emitted by the string builder;
        // the linear chain continues from the "Completed" exec-out.
        if (node->type_id == "builtin.WhileLoop") {
            uint64_t completed_pin = FindExecOutPin(g, next_node, "Completed");
            if (completed_pin == 0) break;
            next_node = NextExecNode(g, completed_pin);
            continue;
        }

        // For all other nodes, follow the first exec-out pin labelled "Out" or
        // any exec-out pin if no "Out" exists.
        uint64_t next_exec_out = 0;
        for (const auto& p : node->pins) {
            if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output) {
                // Prefer the pin named "Out"
                if (p.name == "Out") { next_exec_out = p.id; break; }
                // Fall back to first exec-out found
                if (next_exec_out == 0) next_exec_out = p.id;
            }
        }

        if (next_exec_out == 0) break; // no exec-out (e.g. Return node)
        next_node = NextExecNode(g, next_exec_out);
    }

    return result;
}

TraversalResult GraphTraversal::TraverseDfs(const graph::ScriptGraph& g,
                                             uint64_t start_exec_out_pin_id) {
    TraversalResult result;
    std::unordered_set<uint64_t> visited;
    std::unordered_set<uint64_t> in_stack;

    std::function<std::string(uint64_t, std::unordered_set<uint64_t>&)> resolve_output;
    resolve_output = [&](uint64_t output_pin_id,
                         std::unordered_set<uint64_t>& resolving) -> std::string {
        const graph::Pin* p = g.FindPin(output_pin_id);
        if (!p) return "None";
        if (!p->value.empty()) return p->value;

        uint64_t src_node_id = graph::PinOwnerNodeId(output_pin_id);
        if (resolving.count(src_node_id)) {
            return "_t" + std::to_string(src_node_id);
        }

        resolving.insert(src_node_id);
        const graph::ScriptNode* src_node = g.FindNode(src_node_id);
        if (!src_node) {
            resolving.erase(src_node_id);
            return "None";
        }

        for (const auto& in_pin : src_node->pins) {
            if (in_pin.flow != graph::PinFlow::Data || in_pin.kind != graph::PinKind::Input)
                continue;
            for (const auto& c : g.connections) {
                if (c.to_pin_id == in_pin.id) {
                    std::string v = resolve_output(c.from_pin_id, resolving);
                    resolving.erase(src_node_id);
                    return v;
                }
            }
        }

        resolving.erase(src_node_id);
        return "_t" + std::to_string(src_node_id);
    };

    std::function<void(uint64_t, uint64_t)> dfs_node;
    dfs_node = [&](uint64_t node_id, uint64_t from_node_id) {
        if (node_id == 0) return;

        if (in_stack.count(node_id)) {
            result.has_cycle = true;
            result.cycle_edges.push_back({from_node_id, node_id});
            return;
        }
        if (visited.count(node_id)) return;

        const graph::ScriptNode* node = g.FindNode(node_id);
        if (!node) return;

        visited.insert(node_id);
        in_stack.insert(node_id);
        result.node_ids.push_back(node_id);

        // Collect resolved data dependencies for this statement node.
        auto& deps = result.data_dependencies[node_id];
        for (const auto& pin : node->pins) {
            if (pin.flow != graph::PinFlow::Data || pin.kind != graph::PinKind::Input)
                continue;

            std::string resolved;
            bool connected = false;
            for (const auto& conn : g.connections) {
                if (conn.to_pin_id == pin.id) {
                    std::unordered_set<uint64_t> resolving;
                    resolved = resolve_output(conn.from_pin_id, resolving);
                    connected = true;
                    break;
                }
            }
            if (!connected)
                resolved = pin.value.empty() ? DefaultLiteralForType(pin.type) : pin.value;

            deps.push_back({pin.name, resolved});
        }

        // Determine deterministic next exec-out pins.
        std::vector<uint64_t> ordered_exec_out;
        auto push_pin_if = [&](const std::string& name) {
            uint64_t pid = FindExecOutPin(g, node_id, name);
            if (pid != 0) ordered_exec_out.push_back(pid);
        };

        if (node->type_id == "builtin.IfElse") {
            push_pin_if("True");
            push_pin_if("False");
            push_pin_if("After");
        } else if (node->type_id == "builtin.WhileLoop") {
            push_pin_if("Loop Body");
            push_pin_if("Completed");
        } else if (node->type_id == "builtin.Sequence") {
            for (const auto& p : node->pins) {
                if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output)
                    ordered_exec_out.push_back(p.id);
            }
        } else {
            uint64_t out = FindExecOutPin(g, node_id, "Out");
            if (out != 0)
                ordered_exec_out.push_back(out);
            for (const auto& p : node->pins) {
                if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output && p.id != out)
                    ordered_exec_out.push_back(p.id);
            }
        }

        for (uint64_t out_pin_id : ordered_exec_out) {
            for (uint64_t next_node_id : NextExecNodes(g, out_pin_id))
                dfs_node(next_node_id, node_id);
        }

        in_stack.erase(node_id);
    };

    for (uint64_t first : NextExecNodes(g, start_exec_out_pin_id))
        dfs_node(first, 0);

    return result;
}

TraversalResult GraphTraversal::TraverseFrom(const graph::ScriptGraph& g,
                                              uint64_t node_id,
                                              uint32_t exec_out_pin_index) {
    const graph::ScriptNode* node = g.FindNode(node_id);
    if (!node) return {};

    uint32_t exec_out_count = 0;
    for (const auto& p : node->pins) {
        if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output) {
            if (exec_out_count == exec_out_pin_index)
                return Traverse(g, p.id);
            ++exec_out_count;
        }
    }
    return {};
}

std::vector<uint64_t> GraphTraversal::FindEventNodes(const graph::ScriptGraph& g) {
    std::vector<uint64_t> result;
    for (const auto& node : g.nodes) {
        // An event node has at least one exec-out pin and NO exec-in pin.
        bool has_exec_in  = false;
        bool has_exec_out = false;
        for (const auto& p : node.pins) {
            if (p.flow == graph::PinFlow::Execution) {
                if (p.kind == graph::PinKind::Input)  has_exec_in  = true;
                if (p.kind == graph::PinKind::Output) has_exec_out = true;
            }
        }
        if (has_exec_out && !has_exec_in)
            result.push_back(node.id);
    }
    return result;
}

uint64_t GraphTraversal::FindExecOutPin(const graph::ScriptGraph& g,
                                         uint64_t node_id,
                                         const std::string& pin_name) {
    const graph::ScriptNode* node = g.FindNode(node_id);
    if (!node) return 0;
    for (const auto& p : node->pins) {
        if (p.flow == graph::PinFlow::Execution &&
            p.kind == graph::PinKind::Output &&
            p.name == pin_name)
            return p.id;
    }
    return 0;
}

} // namespace codegen
