#pragma once

#include "graph/ScriptGraph.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace codegen {

// Result of following a single exec-flow chain from one exec-out pin.
// Does NOT recursively descend into the sub-chains of If/Else or While nodes —
// those are handled by PapyrusStringBuilder when it encounters those node types.
struct TraversalResult {
    std::vector<uint64_t>                    node_ids;   // ordered
    bool                                     has_cycle = false;
    std::vector<std::pair<uint64_t,uint64_t>> cycle_edges; // (from_node, to_node)
};

class GraphTraversal {
public:
    // Follow exec-flow starting from a specific exec-OUT pin.
    // Stops when exec-flow terminates or a cycle is detected.
    // Does not descend into branches (IfElse True/False, WhileLoop body).
    static TraversalResult Traverse(const graph::ScriptGraph& g,
                                    uint64_t start_exec_out_pin_id);

    // Convenience: traverse from an exec-out pin by index within a node.
    static TraversalResult TraverseFrom(const graph::ScriptGraph& g,
                                        uint64_t node_id,
                                        uint32_t exec_out_pin_index);

    // Returns all Event node IDs (nodes whose first exec pin is an Output,
    // i.e. they have no ExecIn — they are entry points).
    static std::vector<uint64_t> FindEventNodes(const graph::ScriptGraph& g);

    // Given a node id and the name of an exec-out pin, return that pin's id.
    // Returns 0 if not found.
    static uint64_t FindExecOutPin(const graph::ScriptGraph& g,
                                   uint64_t node_id,
                                   const std::string& pin_name);
};

} // namespace codegen
