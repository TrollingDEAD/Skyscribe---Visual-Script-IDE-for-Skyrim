#pragma once

#include "compiler/CompilerWrapper.h"
#include "compiler/CompilerLine.h"

#include <vector>
#include <mutex>
#include <atomic>

namespace compiler {

// Singleton that owns the CompilerWrapper and a thread-safe output queue.
// The UI thread reads from GetLines() each frame.
class CompileSession {
public:
    static CompileSession& Get();

    // Start an async compile on a background thread.
    // No-op if already running.
    void StartAsync(const std::string& script_path);

    // Called from the UI thread — signals cancellation.
    void Cancel();

    bool IsRunning() const { return wrapper_.IsRunning(); }

    // Atomically drain the incoming line queue into the display buffer.
    // Call once per frame on the UI thread.
    void DrainQueue();

    // Read-only access to the drained display lines.
    const std::vector<CompilerLine>& GetLines() const { return display_lines_; }

    // Last exit code from the most recent compile (INT_MIN if never run).
    int LastExitCode() const { return last_exit_code_.load(); }

    void ClearLines();

private:
    CompileSession() = default;
    CompileSession(const CompileSession&) = delete;
    CompileSession& operator=(const CompileSession&) = delete;

    CompilerWrapper           wrapper_;
    std::mutex                queue_mutex_;
    std::vector<CompilerLine> queue_;         // written by background thread
    std::vector<CompilerLine> display_lines_; // drained on UI thread
    std::atomic<int>          last_exit_code_{ INT_MIN };
};

} // namespace compiler
