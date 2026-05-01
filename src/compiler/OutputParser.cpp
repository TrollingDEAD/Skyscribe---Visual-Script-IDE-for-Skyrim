#include "compiler/OutputParser.h"

#include <regex>

namespace compiler {

// Regex patterns for PapyrusCompiler.exe output lines.
// Format: <path>(<line>,<col>): error: <msg>
//         <path>(<line>,<col>): warning: <msg>
//         Assembly of <name> succeeded
static const std::regex kErrorRegex(
    R"(^(.+)\((\d+),\d+\): error: (.+)$)",
    std::regex::icase);

static const std::regex kWarningRegex(
    R"(^(.+)\((\d+),\d+\): warning: (.+)$)",
    std::regex::icase);

static const std::regex kSuccessRegex(
    R"(^Assembly of .+ succeeded$)",
    std::regex::icase);

CompilerLine OutputParser::Parse(const std::string& raw_line) {
    CompilerLine result;
    result.text = raw_line;

    std::smatch m;

    if (std::regex_match(raw_line, m, kErrorRegex)) {
        result.kind        = LineKind::Error;
        result.file        = m[1].str();
        result.line_number = std::stoi(m[2].str());
        return result;
    }

    if (std::regex_match(raw_line, m, kWarningRegex)) {
        result.kind        = LineKind::Warning;
        result.file        = m[1].str();
        result.line_number = std::stoi(m[2].str());
        return result;
    }

    if (std::regex_match(raw_line, kSuccessRegex)) {
        result.kind = LineKind::Success;
        return result;
    }

    result.kind = LineKind::Info;
    return result;
}

} // namespace compiler
