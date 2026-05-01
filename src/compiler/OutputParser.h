#pragma once

#include "compiler/CompilerLine.h"
#include <string>

namespace compiler {

class OutputParser {
public:
    // Parse a single raw stdout/stderr line from PapyrusCompiler.exe.
    // Never throws.
    static CompilerLine Parse(const std::string& raw_line);
};

} // namespace compiler
