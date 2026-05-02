#include "codegen/PapyrusStringBuilder.h"
#include "codegen/GraphTraversal.h"
#include "graph/NodeRegistry.h"

#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace codegen {

// ─── Type helpers ─────────────────────────────────────────────────────────────

static std::string PapyrusTypeName(graph::PinType t) {
    switch (t) {
        case graph::PinType::Bool:              return "Bool";
        case graph::PinType::Int:               return "Int";
        case graph::PinType::Float:             return "Float";
        case graph::PinType::String:            return "String";
        case graph::PinType::ObjectRef:         return "ObjectReference";
        case graph::PinType::Actor:             return "Actor";
        case graph::PinType::Quest:             return "Quest";
        case graph::PinType::Form:              return "Form";
        case graph::PinType::Array_Bool:        return "Bool[]";
        case graph::PinType::Array_Int:         return "Int[]";
        case graph::PinType::Array_Float:       return "Float[]";
        case graph::PinType::Array_String:      return "String[]";
        case graph::PinType::Array_ObjectRef:   return "ObjectReference[]";
        default:                                return "Auto";
    }
}

static std::string DefaultLiteralForType(graph::PinType t) {
    switch (t) {
        case graph::PinType::Bool:  return "False";
        case graph::PinType::Int:   return "0";
        case graph::PinType::Float: return "0.0";
        case graph::PinType::String: return "\"\"";
        default:                    return "None";
    }
}

// ─── Data-flow resolution ─────────────────────────────────────────────────────

// Forward declarations
static std::string SubstituteTemplate(const graph::ScriptGraph& g,
                                       uint64_t node_id,
                                       const std::string& tmpl,
                                       std::unordered_set<uint64_t>& resolving);

// Resolve the expression produced by a specific data-output pin (from_pin_id).
static std::string ResolveOutput(const graph::ScriptGraph& g,
                                  uint64_t from_pin_id,
                                  std::unordered_set<uint64_t>& resolving) {
    uint64_t src_id = graph::PinOwnerNodeId(from_pin_id);
    if (resolving.count(src_id)) return "None"; // cycle guard

    const graph::ScriptNode* src = g.FindNode(src_id);
    if (!src) return "None";

    const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(src->type_id);
    if (!def) {
        const graph::Pin* p = g.FindPin(from_pin_id);
        return (p && !p->value.empty()) ? p->value : "None";
    }

    if (!def->codegen_template.empty()) {
        return SubstituteTemplate(g, src_id, def->codegen_template, resolving);
    }

    // No template: event parameter nodes or GetVariable-style nodes.
    // Return the pin's stored value if set, otherwise the pin name (for event params).
    const graph::Pin* p = g.FindPin(from_pin_id);
    if (!p) return "None";
    return (!p->value.empty()) ? p->value : p->name;
}

// Substitute all {PinName} tokens in tmpl with the resolved expressions.
static std::string SubstituteTemplate(const graph::ScriptGraph& g,
                                       uint64_t node_id,
                                       const std::string& tmpl,
                                       std::unordered_set<uint64_t>& resolving) {
    resolving.insert(node_id);
    std::string out;
    out.reserve(tmpl.size());

    for (size_t i = 0; i < tmpl.size(); ) {
        if (tmpl[i] == '{') {
            size_t close = tmpl.find('}', i + 1);
            if (close != std::string::npos) {
                std::string pin_name = tmpl.substr(i + 1, close - i - 1);

                // Find the data-input pin by name on node_id
                const graph::ScriptNode* node = g.FindNode(node_id);
                std::string resolved;

                if (node) {
                    // Look for the data-input pin
                    bool found_pin = false;
                    for (const auto& pin : node->pins) {
                        if (pin.name == pin_name &&
                            pin.flow == graph::PinFlow::Data &&
                            pin.kind == graph::PinKind::Input) {
                            found_pin = true;
                            // Check for incoming data connection
                            bool connected = false;
                            for (const auto& conn : g.connections) {
                                if (conn.to_pin_id == pin.id) {
                                    resolved = ResolveOutput(g, conn.from_pin_id, resolving);
                                    connected = true;
                                    break;
                                }
                            }
                            if (!connected) {
                                // Fall back to pin instance value, then def default, then type zero
                                if (!pin.value.empty()) {
                                    resolved = pin.value;
                                } else {
                                    const graph::NodeDefinition* def =
                                        graph::NodeRegistry::Get().Find(node->type_id);
                                    if (def) {
                                        for (const auto& pd : def->pins) {
                                            if (pd.name == pin_name &&
                                                pd.flow == graph::PinFlow::Data &&
                                                pd.kind == graph::PinKind::Input) {
                                                resolved = pd.default_value.empty()
                                                    ? DefaultLiteralForType(pd.type)
                                                    : pd.default_value;
                                                break;
                                            }
                                        }
                                    }
                                    if (resolved.empty()) resolved = DefaultLiteralForType(pin.type);
                                }
                            }
                            break;
                        }
                    }
                    if (!found_pin) resolved = pin_name; // token passed through unchanged
                }

                out += resolved;
                i = close + 1;
                continue;
            }
        }
        out += tmpl[i++];
    }

    resolving.erase(node_id);
    return out;
}

// ─── Emit helpers ─────────────────────────────────────────────────────────────

// Forward declarations
static void EmitStatement(const graph::ScriptGraph& g, uint64_t node_id,
                           const std::string& indent, std::ostream& out);

static void EmitBranch(const graph::ScriptGraph& g, uint64_t exec_out_pin_id,
                        const std::string& indent, std::ostream& out) {
    auto result = GraphTraversal::Traverse(g, exec_out_pin_id);
    for (uint64_t nid : result.node_ids)
        EmitStatement(g, nid, indent, out);
}

static void EmitStatement(const graph::ScriptGraph& g, uint64_t node_id,
                           const std::string& indent, std::ostream& out) {
    const graph::ScriptNode* node = g.FindNode(node_id);
    if (!node) return;
    const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(node->type_id);
    if (!def) return;

    // ── If / Else ──────────────────────────────────────────────────────────
    if (node->type_id == "builtin.IfElse") {
        // Resolve the Condition data-input pin directly and wrap in parens
        std::unordered_set<uint64_t> res;
        std::string cond;
        for (const auto& pin : node->pins) {
            if (pin.name == "Condition" && pin.flow == graph::PinFlow::Data &&
                pin.kind == graph::PinKind::Input) {
                bool connected = false;
                for (const auto& conn : g.connections) {
                    if (conn.to_pin_id == pin.id) {
                        cond = ResolveOutput(g, conn.from_pin_id, res);
                        connected = true; break;
                    }
                }
                if (!connected)
                    cond = pin.value.empty() ? "False" : pin.value;
                break;
            }
        }
        out << indent << "If (" << cond << ")\n";

        uint64_t true_pin  = GraphTraversal::FindExecOutPin(g, node_id, "True");
        uint64_t false_pin = GraphTraversal::FindExecOutPin(g, node_id, "False");

        if (true_pin)  EmitBranch(g, true_pin, indent + "    ", out);

        // Only emit Else if the False branch is connected
        bool false_used = false;
        if (false_pin) {
            for (const auto& conn : g.connections)
                if (conn.from_pin_id == false_pin) { false_used = true; break; }
        }
        if (false_used) {
            out << indent << "Else\n";
            EmitBranch(g, false_pin, indent + "    ", out);
        }
        out << indent << "EndIf\n";
        return;
    }

    // ── While Loop ─────────────────────────────────────────────────────────
    if (node->type_id == "builtin.WhileLoop") {
        std::unordered_set<uint64_t> res;
        std::string cond;
        for (const auto& pin : node->pins) {
            if (pin.name == "Condition" && pin.flow == graph::PinFlow::Data &&
                pin.kind == graph::PinKind::Input) {
                bool connected = false;
                for (const auto& conn : g.connections) {
                    if (conn.to_pin_id == pin.id) {
                        cond = ResolveOutput(g, conn.from_pin_id, res);
                        connected = true; break;
                    }
                }
                if (!connected)
                    cond = pin.value.empty() ? "False" : pin.value;
                break;
            }
        }
        out << indent << "While (" << cond << ")\n";

        uint64_t body_pin = GraphTraversal::FindExecOutPin(g, node_id, "Loop Body");
        if (body_pin) EmitBranch(g, body_pin, indent + "    ", out);

        out << indent << "EndWhile\n";
        return;
    }

    // ── Sequence ───────────────────────────────────────────────────────────
    if (node->type_id == "builtin.Sequence") {
        for (const auto& pin : node->pins) {
            if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Output)
                EmitBranch(g, pin.id, indent, out);
        }
        return;
    }

    // ── Return ─────────────────────────────────────────────────────────────
    if (node->type_id == "builtin.Return") {
        out << indent << "Return\n";
        return;
    }

    // ── DeclareLocal (hoisted — skip inline) ───────────────────────────────
    if (node->type_id == "builtin.DeclareLocal") {
        return;
    }

    // ── General statement with codegen_template ────────────────────────────
    if (!def->codegen_template.empty()) {
        std::unordered_set<uint64_t> res;
        std::string stmt = SubstituteTemplate(g, node_id, def->codegen_template, res);
        out << indent << stmt << "\n";
    }
}

// ─── DeclareLocal hoisting ────────────────────────────────────────────────────

// Collect every DeclareLocal node reachable via data connections from exec-chain nodes.
static void CollectDeclareLocals(const graph::ScriptGraph& g,
                                  const std::vector<uint64_t>& chain_nodes,
                                  std::vector<uint64_t>& out_local_ids,
                                  std::unordered_set<uint64_t>& visited_data) {
    std::function<void(uint64_t)> scan = [&](uint64_t nid) {
        const graph::ScriptNode* n = g.FindNode(nid);
        if (!n) return;
        for (const auto& pin : n->pins) {
            if (pin.flow != graph::PinFlow::Data || pin.kind != graph::PinKind::Input)
                continue;
            for (const auto& conn : g.connections) {
                if (conn.to_pin_id != pin.id) continue;
                uint64_t src = graph::PinOwnerNodeId(conn.from_pin_id);
                if (visited_data.count(src)) continue;
                visited_data.insert(src);
                const graph::ScriptNode* sn = g.FindNode(src);
                if (!sn) continue;
                if (sn->type_id == "builtin.DeclareLocal")
                    out_local_ids.push_back(src);
                scan(src);
            }
        }
    };
    for (uint64_t nid : chain_nodes)
        scan(nid);
}

// Deeply scan an entire exec-chain (including branches) for DeclareLocals.
static void CollectAllDeclareLocals(const graph::ScriptGraph& g,
                                     uint64_t start_exec_pin,
                                     std::vector<uint64_t>& out_local_ids,
                                     std::unordered_set<uint64_t>& visited_data) {
    auto result = GraphTraversal::Traverse(g, start_exec_pin);
    CollectDeclareLocals(g, result.node_ids, out_local_ids, visited_data);

    // Recurse into If/Else and While branches
    for (uint64_t nid : result.node_ids) {
        const graph::ScriptNode* n = g.FindNode(nid);
        if (!n) continue;
        for (const auto& pin : n->pins) {
            if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Output) {
                // Skip "Out" / "Completed" — those are already in the main chain result
                // Only recurse into branch pins
                if (n->type_id == "builtin.IfElse" ||
                    (n->type_id == "builtin.WhileLoop" && pin.name == "Loop Body") ||
                    n->type_id == "builtin.Sequence") {
                    CollectAllDeclareLocals(g, pin.id, out_local_ids, visited_data);
                }
            }
        }
    }
}

// ─── Event header ─────────────────────────────────────────────────────────────

// Build the Papyrus event signature, e.g. "Event OnDeath(Actor akKiller)"
static std::string EventHeader(const graph::ScriptNode& event_node,
                                const graph::NodeDefinition& def) {
    // Papyrus event name is the display_name (e.g. "OnInit", "OnDeath", etc.)
    std::string header = "Event " + def.display_name + "(";
    bool first = true;
    for (const auto& pin : event_node.pins) {
        if (pin.flow == graph::PinFlow::Data && pin.kind == graph::PinKind::Output) {
            if (!first) header += ", ";
            header += PapyrusTypeName(pin.type) + " " + pin.name;
            first = false;
        }
    }
    header += ")";
    return header;
}

// ─── Event body ───────────────────────────────────────────────────────────────

static void EmitEventBody(const graph::ScriptGraph& g,
                           uint64_t event_node_id,
                           std::ostream& out) {
    const graph::ScriptNode* event_node = g.FindNode(event_node_id);
    if (!event_node) return;

    // Find the exec-out pin of the event node
    uint64_t exec_pin = 0;
    for (const auto& pin : event_node->pins) {
        if (pin.flow == graph::PinFlow::Execution && pin.kind == graph::PinKind::Output) {
            exec_pin = pin.id;
            break;
        }
    }

    // Collect all DeclareLocal vars reachable from this event's chain
    std::vector<uint64_t> local_ids;
    std::unordered_set<uint64_t> visited_data;
    if (exec_pin) CollectAllDeclareLocals(g, exec_pin, local_ids, visited_data);

    // Emit hoisted local declarations at the top
    for (uint64_t lid : local_ids) {
        const graph::ScriptNode* ln = g.FindNode(lid);
        if (!ln) continue;
        // The variable name comes from the "Ref" pin's value; fall back to "kLocal"
        std::string var_name = "kLocal";
        for (const auto& p : ln->pins) {
            if (p.name == "Ref" && !p.value.empty()) { var_name = p.value; break; }
        }
        out << "    Auto " << var_name << "\n";
    }

    // Emit exec-flow statements
    if (exec_pin) EmitBranch(g, exec_pin, "    ", out);
}

// ─── Property emission ────────────────────────────────────────────────────────

static void EmitProperties(const graph::ScriptGraph& g, std::ostream& out) {
    for (const auto& prop : g.properties) {
        std::string kind_kw;
        switch (prop.kind) {
            case graph::PropertyKind::Auto:         kind_kw = "Auto";         break;
            case graph::PropertyKind::AutoReadOnly: kind_kw = "AutoReadOnly"; break;
            case graph::PropertyKind::Conditional:  kind_kw = "Conditional";  break;
        }

        out << PapyrusTypeName(prop.type) << " Property " << prop.name
            << " " << kind_kw;

        if (!prop.default_value.empty())
            out << " = " << prop.default_value;

        out << "\n";
    }
    if (!g.properties.empty()) out << "\n";
}

// ─── Main entry ───────────────────────────────────────────────────────────────

PapyrusStringBuilder::Result PapyrusStringBuilder::Generate(const graph::ScriptGraph& g) {
    Result result;

    // ── Edge cases ────────────────────────────────────────────────────────────
    if (g.script_name.empty()) {
        result.has_errors    = true;
        result.error_message = "Script name is empty";
        // Still generate something minimal
        result.source = "; ERROR: Script name is empty\nScriptname UNNAMED\n";
        return result;
    }

    std::ostringstream out;

    // ── Script header ─────────────────────────────────────────────────────────
    out << "Scriptname " << g.script_name;
    if (!g.extends.empty()) out << " extends " << g.extends;
    out << "\n";

    // ── Import statements (task 3.17) ─────────────────────────────────────────
    {
        std::set<std::string> imports;
        for (const auto& sn : g.nodes) {
            const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(sn.type_id);
            if (def && !def->source_script.empty())
                imports.insert(def->source_script);
        }
        if (!imports.empty()) {
            for (const auto& imp : imports)
                out << "Import " << imp << "\n";
        }
    }
    out << "\n";

    // ── Properties ────────────────────────────────────────────────────────────
    EmitProperties(g, out);

    // ── Functions (task 3.9) ──────────────────────────────────────────────────
    for (const auto& func : g.functions) {
        // Function header: "Float Function GetMod()" or "Function DoThing()"
        if (func.return_type != graph::PinType::Unknown)
            out << PapyrusTypeName(func.return_type) << " ";
        out << "Function " << func.name << "(";
        for (size_t i = 0; i < func.parameters.size(); ++i) {
            if (i) out << ", ";
            out << PapyrusTypeName(func.parameters[i].type)
                << " " << func.parameters[i].name;
        }
        out << ")";
        if (func.is_global) out << " Global";
        out << "\n";

        // Emit body graph (same as event body)
        if (func.body_graph) {
            // Find the function entry node in the body graph
            auto entry_id_str = "script." + g.script_name + ".entry." + func.name;
            for (const auto& sn : func.body_graph->nodes) {
                if (sn.type_id == entry_id_str) {
                    EmitEventBody(*func.body_graph, sn.id, out);
                    break;
                }
            }
        }
        out << "EndFunction\n\n";
    }

    // ── Events ────────────────────────────────────────────────────────────────
    auto event_ids = GraphTraversal::FindEventNodes(g);

    if (event_ids.empty() && g.nodes.empty()) {
        // Empty graph — that's valid, just no events
    }

    for (uint64_t eid : event_ids) {
        const graph::ScriptNode* en = g.FindNode(eid);
        if (!en) continue;
        const graph::NodeDefinition* def = graph::NodeRegistry::Get().Find(en->type_id);
        if (!def) continue;

        out << EventHeader(*en, *def) << "\n";
        EmitEventBody(g, eid, out);
        out << "EndEvent\n\n";
    }

    result.source = out.str();
    return result;
}

} // namespace codegen
