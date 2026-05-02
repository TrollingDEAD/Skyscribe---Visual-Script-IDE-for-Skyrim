#pragma once

#include "graph/ScriptGraph.h"
#include <cstdint>
#include <string>
#include <vector>

namespace codegen {

enum class LintSeverity { Warning, Error };

struct LintDiagnostic {
    LintSeverity severity   = LintSeverity::Error;
    uint64_t     node_id    = 0;  // 0 = graph-level diagnostic
    std::string  rule_id;         // e.g. "L01"
    std::string  message;
};

class LintPass {
public:
    // Run all lint rules against the graph.
    // Returns a list of diagnostics; empty means the graph is clean.
    static std::vector<LintDiagnostic> Run(const graph::ScriptGraph& g);

    // Returns true if any Error-severity diagnostic is present.
    static bool HasErrors(const std::vector<LintDiagnostic>& diags);
};

} // namespace codegen
