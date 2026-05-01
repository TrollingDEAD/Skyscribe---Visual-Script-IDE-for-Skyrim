#pragma once

#include "graph/PinType.h"
#include <string>
#include <vector>

namespace graph {

// ── Pin direction & flow ──────────────────────────────────────────────────────

enum class PinKind { Input, Output };
enum class PinFlow { Execution, Data };

// ── Node categories ───────────────────────────────────────────────────────────

enum class NodeCategory {
    Event,
    ControlFlow,
    Variable,
    Math,
    Debug,
    Actor,
    Quest,
    Utility,
    Custom
};

// ── Static definition types (live in NodeRegistry) ───────────────────────────

struct PinDefinition {
    std::string name;
    PinKind     kind;
    PinFlow     flow;
    PinType     type;
    std::string default_value;  // used when pin is unconnected
    std::string tooltip;
};

struct NodeDefinition {
    std::string              type_id;       // stable key, e.g. "builtin.OnInit"
    std::string              display_name;
    NodeCategory             category;
    std::string              tooltip;
    std::string              source_script; // empty for built-ins
    std::vector<PinDefinition> pins;
    std::string              codegen_template; // {pin_name} substitution tokens
};

} // namespace graph
