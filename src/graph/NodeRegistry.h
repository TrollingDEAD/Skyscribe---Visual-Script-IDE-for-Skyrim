#pragma once

#include "graph/NodeDefinition.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace graph {

class NodeRegistry {
public:
    static NodeRegistry& Get();

    // Register a node definition. Overwrites if type_id already exists.
    void Register(NodeDefinition def);

    // Returns nullptr if type_id is not registered.
    const NodeDefinition* Find(const std::string& type_id) const;

    // All registered definitions, sorted by category then display_name.
    const std::vector<NodeDefinition>& AllNodes() const;

    // All definitions for a specific category.
    std::vector<const NodeDefinition*> ByCategory(NodeCategory category) const;

    // Total number of registered nodes.
    size_t Count() const { return all_.size(); }

private:
    NodeRegistry() = default;
    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    void RebuildSorted();

    std::unordered_map<std::string, NodeDefinition> map_;
    std::vector<NodeDefinition>                     all_; // sorted cache
    bool                                            dirty_ = false;
};

} // namespace graph
