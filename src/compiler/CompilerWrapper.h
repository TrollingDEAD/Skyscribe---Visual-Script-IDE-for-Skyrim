#pragma once

#include "compiler/CompilerLine.h"

#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace compiler {

struct CompileResult {
    bool                      success   = false;
    int                       exit_code = -1;
    std::vector<CompilerLine> lines;
};

// Launches PapyrusCompiler.exe via CreateProcess, captures stdout/stderr on a
// background thread, and calls the provided callback for each line.
// All public methods are thread-safe with respect to Cancel().
class CompilerWrapper {
public:
    CompilerWrapper() = default;
    ~CompilerWrapper() = default;

    // Build the full command line string from Settings + script_path.
    // Returns "" if settings are incomplete.
    std::string BuildArgs(const std::string& script_path) const;

    // Synchronously compile script_path.
    // line_callback is called on the background reader thread — post to the UI
    // queue rather than touching ImGui directly from it.
    // Returns when the process exits, times out, or is cancelled.
    CompileResult Invoke(
        const std::string& script_path,
        std::function<void(const CompilerLine&)> line_callback = nullptr);

    // Signal an in-flight Invoke() to terminate the child process.
    void Cancel();

    bool IsRunning() const { return running_.load(); }

private:
    std::atomic<bool>    running_   { false };
    std::atomic<bool>    cancelled_ { false };

    // Win32 HANDLE stored as void* to avoid pulling Windows.h into this header.
    void* process_handle_ = nullptr;
};

} // namespace compiler
