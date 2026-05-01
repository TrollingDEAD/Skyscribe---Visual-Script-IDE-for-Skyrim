#pragma once

#include "graph/ScriptGraph.h"
#include <nlohmann/json.hpp>

namespace graph {

struct GraphSerializer {
    // Serialize a ScriptGraph to JSON.
    static nlohmann::json Save(const ScriptGraph& g);

    // Deserialize a ScriptGraph from JSON.
    // Nodes with unknown type_id are skipped (forward-compatibility).
    // Requires NodeRegistry to be populated before calling.
    static ScriptGraph Load(const nlohmann::json& j);
};

} // namespace graph
