#include "graph/NodeRegistry.h"
#include <algorithm>

namespace graph {

NodeRegistry& NodeRegistry::Get() {
    static NodeRegistry instance;
    return instance;
}

void NodeRegistry::Register(NodeDefinition def) {
    std::string key = def.type_id;
    map_[key] = std::move(def);
    dirty_ = true;
}

void NodeRegistry::Unregister(const std::string& type_id) {
    if (map_.erase(type_id))
        dirty_ = true;
}

const NodeDefinition* NodeRegistry::Find(const std::string& type_id) const {
    auto it = map_.find(type_id);
    return it != map_.end() ? &it->second : nullptr;
}

const std::vector<NodeDefinition>& NodeRegistry::AllNodes() const {
    if (dirty_) const_cast<NodeRegistry*>(this)->RebuildSorted();
    return all_;
}

std::vector<const NodeDefinition*> NodeRegistry::ByCategory(NodeCategory category) const {
    std::vector<const NodeDefinition*> result;
    for (const auto& def : AllNodes())
        if (def.category == category)
            result.push_back(&def);
    return result;
}

void NodeRegistry::RebuildSorted() {
    all_.clear();
    all_.reserve(map_.size());
    for (const auto& kv : map_)
        all_.push_back(kv.second);

    // Sort by category (enum order) then display_name.
    std::sort(all_.begin(), all_.end(), [](const NodeDefinition& a, const NodeDefinition& b) {
        if (a.category != b.category)
            return static_cast<int>(a.category) < static_cast<int>(b.category);
        return a.display_name < b.display_name;
    });
    dirty_ = false;
}

} // namespace graph
