#pragma once

#include "graph/ScriptGraph.h"
#include <string>

namespace codegen {

class PapyrusStringBuilder {
public:
    struct Result {
        std::string source;
        bool        has_errors = false;
        std::string error_message; // first fatal error encountered, if any
    };

    // Generate a full .psc script text from the given ScriptGraph.
    static Result Generate(const graph::ScriptGraph& g);
};

} // namespace codegen
