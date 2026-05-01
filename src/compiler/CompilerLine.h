#pragma once

#include <string>

namespace compiler {

enum class LineKind { Info, Warning, Error, Success };

struct CompilerLine {
    LineKind    kind        = LineKind::Info;
    std::string file;        // populated for Error / Warning
    int         line_number = 0; // 0 if not applicable
    std::string text;        // full raw line
};

} // namespace compiler
