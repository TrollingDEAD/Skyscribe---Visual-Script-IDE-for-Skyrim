#pragma once

namespace graph {

// Registers all 61 built-in nodes from ROADMAP §8 into NodeRegistry.
// Called once from Application::Init() before any UI renders.
struct BuiltinNodes {
    static void RegisterAll();
};

} // namespace graph
