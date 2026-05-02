#include "codegen/LintPass.h"
#include "codegen/GraphTraversal.h"
#include "graph/NodeRegistry.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace codegen {

namespace {

// Helper: add a diagnostic.
void Add(std::vector<LintDiagnostic>& out,
         LintSeverity sev, uint64_t node_id,
         const char* rule_id, const std::string& msg) {
    out.push_back({sev, node_id, rule_id, msg});
}

// ─ L01: Empty script name ─────────────────────────────────────────────────────
void L01_EmptyScriptName(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    if (g.script_name.empty())
        Add(out, LintSeverity::Error, 0, "L01", "Script name is empty.");
}

// ─ L02: Unknown node type ─────────────────────────────────────────────────────
void L02_UnknownNodeType(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    for (const auto& node : g.nodes) {
        if (!graph::NodeRegistry::Get().Find(node.type_id))
            Add(out, LintSeverity::Error, node.id, "L02",
                "Unknown node type: " + node.type_id);
    }
}

// ─ L03: Disconnected required data input ─────────────────────────────────────
// A "required" data input is one whose default_value is empty and whose type is
// not Unknown (Unknown-typed pins are optional catch-alls).
void L03_DisconnectedRequiredInput(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    for (const auto& node : g.nodes) {
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node.type_id);
        if (!def) continue;

        for (size_t i = 0; i < node.pins.size(); ++i) {
            const auto& pin = node.pins[i];
            if (pin.flow != graph::PinFlow::Data || pin.kind != graph::PinKind::Input) continue;

            // Get the definition's default_value for this pin
            const graph::PinDefinition* pdef = (i < def->pins.size()) ? &def->pins[i] : nullptr;
            bool has_default = (pdef && !pdef->default_value.empty()) || !pin.value.empty();
            bool is_optional = (pin.type == graph::PinType::Unknown);

            if (!has_default && !is_optional) {
                // Check if connected
                bool connected = false;
                for (const auto& conn : g.connections)
                    if (conn.to_pin_id == pin.id) { connected = true; break; }

                if (!connected)
                    Add(out, LintSeverity::Warning, node.id, "L03",
                        "Required input pin '" + pin.name + "' is not connected.");
            }
        }
    }
}

// ─ L04: Execution cycle ───────────────────────────────────────────────────────
void L04_ExecutionCycle(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    auto event_ids = GraphTraversal::FindEventNodes(g);
    for (uint64_t eid : event_ids) {
        const graph::ScriptNode* en = g.FindNode(eid);
        if (!en) continue;
        for (const auto& pin : en->pins) {
            if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Output) {
                auto result = GraphTraversal::Traverse(g, pin.id);
                if (result.has_cycle) {
                    for (const auto& [from_id, to_id] : result.cycle_edges)
                        Add(out, LintSeverity::Error, from_id, "L04",
                            "Execution cycle detected.");
                }
            }
        }
    }
}

// ─ L05: Isolated node (no connections at all) ────────────────────────────────
void L05_IsolatedNode(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    // Build set of all node IDs that have at least one connection
    std::unordered_set<uint64_t> connected_nodes;
    for (const auto& conn : g.connections) {
        connected_nodes.insert(graph::PinOwnerNodeId(conn.from_pin_id));
        connected_nodes.insert(graph::PinOwnerNodeId(conn.to_pin_id));
    }

    for (const auto& node : g.nodes) {
        if (!connected_nodes.count(node.id)) {
            // Ignore pure event nodes with no pins other than exec-out — those are
            // legitimately standalone (e.g., OnInit in an empty script)
            const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node.type_id);
            if (def && def->category == graph::NodeCategory::Event) continue;

            Add(out, LintSeverity::Warning, node.id, "L05",
                "Node '" + (def ? def->display_name : node.type_id) +
                "' has no connections and will not produce output.");
        }
    }
}

// ─ L06: No event nodes ───────────────────────────────────────────────────────
void L06_NoEventNodes(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    if (g.nodes.empty()) return; // empty graph — not an error
    auto events = GraphTraversal::FindEventNodes(g);
    if (events.empty())
        Add(out, LintSeverity::Warning, 0, "L06",
            "Graph has no event nodes — no code will be generated.");
}

// ─ L07: DivideInt with literal 0 denominator ─────────────────────────────────
void L07_DivisionByZero(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    for (const auto& node : g.nodes) {
        if (node.type_id != "builtin.DivideInt" && node.type_id != "builtin.Modulo") continue;
        for (const auto& pin : node.pins) {
            if (pin.name != "B" || pin.flow != graph::PinFlow::Data ||
                pin.kind != graph::PinKind::Input) continue;
            // Not connected — use stored value
            bool connected = false;
            for (const auto& conn : g.connections)
                if (conn.to_pin_id == pin.id) { connected = true; break; }
            if (!connected && (pin.value == "0" || pin.value == "")) {
                Add(out, LintSeverity::Warning, node.id, "L07",
                    "Potential division by zero: B input is 0 or unset.");
            }
        }
    }
}

// ─ L08: Duplicate event type ─────────────────────────────────────────────────
void L08_DuplicateEvent(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    std::unordered_map<std::string, int> event_counts;
    for (const auto& node : g.nodes) {
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node.type_id);
        if (def && def->category == graph::NodeCategory::Event)
            event_counts[node.type_id]++;
    }
    for (const auto& node : g.nodes) {
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node.type_id);
        if (def && def->category == graph::NodeCategory::Event &&
            event_counts[node.type_id] > 1)
            Add(out, LintSeverity::Error, node.id, "L08",
                "Duplicate event '" + def->display_name +
                "' — Papyrus does not support multiple handlers for the same event.");
    }
}

// ─ L09: Script name has spaces or special characters ─────────────────────────
void L09_InvalidScriptName(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    if (g.script_name.empty()) return;
    for (char c : g.script_name) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
            Add(out, LintSeverity::Error, 0, "L09",
                "Script name '" + g.script_name +
                "' contains invalid characters. Use letters, digits, and underscores only.");
            return;
        }
    }
    if (isdigit(static_cast<unsigned char>(g.script_name[0])))
        Add(out, LintSeverity::Error, 0, "L09",
            "Script name '" + g.script_name + "' must not start with a digit.");
}

// ─ L10: Property name collision with script name ─────────────────────────────
void L10_PropertyNameCollision(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    std::unordered_set<std::string> names;
    for (const auto& prop : g.properties) {
        if (names.count(prop.name))
            Add(out, LintSeverity::Error, 0, "L10",
                "Duplicate property name '" + prop.name + "'.");
        names.insert(prop.name);
    }
}

// ─ L11: Empty property name ──────────────────────────────────────────────────
void L11_EmptyPropertyName(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    for (const auto& prop : g.properties) {
        if (prop.name.empty())
            Add(out, LintSeverity::Error, 0, "L11", "A property has an empty name.");
    }
}

// ─ L12: Script extends itself ─────────────────────────────────────────────────
void L12_SelfExtend(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    if (!g.extends.empty() && g.extends == g.script_name)
        Add(out, LintSeverity::Error, 0, "L12",
            "Script '" + g.script_name + "' cannot extend itself.");
}

// ─ L13: Unreachable exec chain (exec-in pin is never connected) ───────────────
void L13_UnreachableNode(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    // Collect all nodes reachable via exec-flow from event nodes
    std::unordered_set<uint64_t> reachable;
    auto event_ids = GraphTraversal::FindEventNodes(g);
    std::function<void(uint64_t)> scan = [&](uint64_t exec_pin) {
        auto result = GraphTraversal::Traverse(g, exec_pin);
        for (uint64_t nid : result.node_ids) {
            if (reachable.count(nid)) continue;
            reachable.insert(nid);
            const graph::ScriptNode* n = g.FindNode(nid);
            if (!n) continue;
            // Recurse into branches
            for (const auto& pin : n->pins) {
                if (pin.flow == graph::PinFlow::Execution &&
                    pin.kind == graph::PinKind::Output &&
                    (n->type_id == "builtin.IfElse" ||
                     (n->type_id == "builtin.WhileLoop" && pin.name == "Loop Body") ||
                     n->type_id == "builtin.Sequence"))
                    scan(pin.id);
            }
        }
    };
    for (uint64_t eid : event_ids) {
        reachable.insert(eid);
        const graph::ScriptNode* en = g.FindNode(eid);
        if (!en) continue;
        for (const auto& pin : en->pins)
            if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Output)
                scan(pin.id);
    }

    for (const auto& node : g.nodes) {
        if (reachable.count(node.id)) continue;
        // Only warn about action nodes (those with exec-in)
        bool has_exec_in = false;
        for (const auto& pin : node.pins)
            if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Input)
                { has_exec_in = true; break; }
        if (!has_exec_in) continue;

        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node.type_id);
        Add(out, LintSeverity::Warning, node.id, "L13",
            "Node '" + (def ? def->display_name : node.type_id) +
            "' is never executed (not reachable from any event).");
    }
}

// ─ L14: LiteralString left with empty value ──────────────────────────────────
void L14_EmptyStringLiteral(const graph::ScriptGraph& g, std::vector<LintDiagnostic>& out) {
    for (const auto& node : g.nodes) {
        if (node.type_id != "builtin.LiteralString") continue;
        for (const auto& pin : node.pins) {
            if (pin.name == "Value" && pin.value.empty()) {
                Add(out, LintSeverity::Warning, node.id, "L14",
                    "LiteralString node has an empty value.");
                break;
            }
        }
    }
}

} // anonymous namespace

// ─── Public API ───────────────────────────────────────────────────────────────

std::vector<LintDiagnostic> LintPass::Run(const graph::ScriptGraph& g) {
    std::vector<LintDiagnostic> diags;
    L01_EmptyScriptName(g, diags);
    L02_UnknownNodeType(g, diags);
    L03_DisconnectedRequiredInput(g, diags);
    L04_ExecutionCycle(g, diags);
    L05_IsolatedNode(g, diags);
    L06_NoEventNodes(g, diags);
    L07_DivisionByZero(g, diags);
    L08_DuplicateEvent(g, diags);
    L09_InvalidScriptName(g, diags);
    L10_PropertyNameCollision(g, diags);
    L11_EmptyPropertyName(g, diags);
    L12_SelfExtend(g, diags);
    L13_UnreachableNode(g, diags);
    L14_EmptyStringLiteral(g, diags);
    return diags;
}

bool LintPass::HasErrors(const std::vector<LintDiagnostic>& diags) {
    for (const auto& d : diags)
        if (d.severity == LintSeverity::Error) return true;
    return false;
}

} // namespace codegen
