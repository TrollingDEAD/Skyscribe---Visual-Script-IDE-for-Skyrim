#include "codegen/GraphTraversal.h"
#include "graph/NodeRegistry.h"

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

// Returns the first exec-OUT pin id of a node whose output is an exec pin
// (used to start traversal from an Event node).
static uint64_t FirstExecOutPin(const graph::ScriptNode& node) {
    for (const auto& p : node.pins) {
        if (p.flow == graph::PinFlow::Execution && p.kind == graph::PinKind::Output)
            return p.id;
    }
    return 0;
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
