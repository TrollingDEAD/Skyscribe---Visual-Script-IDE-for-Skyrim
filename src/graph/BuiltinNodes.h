#pragma once

#include "graph/ScriptGraph.h"  // PropertyDefinition
#include <string>
#include <vector>

namespace graph {

// Registers all 61 built-in nodes from ROADMAP §8 into NodeRegistry.
// Called once from Application::Init() before any UI renders.
struct BuiltinNodes {
    static void RegisterAll();

    // Register (or re-register) Get/Set Property nodes for one script.
    // Removes all previously registered nodes with type_id prefix
    // "script.<script_name>." before re-adding from props.
    // Pass an empty props vector to just remove existing entries.
    static void SyncPropertyNodes(const std::string& script_name,
                                   const std::vector<PropertyDefinition>& props);

    // Remove all dynamic function nodes (entry/return/call) for a script.
    // Called on project close or when rebuilding from serialised data.
    static void RemoveFunctionNodes(const std::string& script_name,
                                     const std::string& func_name);
};

} // namespace graph
