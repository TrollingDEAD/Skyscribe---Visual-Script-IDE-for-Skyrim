#include "compiler/CompileSession.h"
#include "app/Logger.h"

#include <thread>
#include <climits>

namespace compiler {

CompileSession& CompileSession::Get() {
    static CompileSession instance;
    return instance;
}

void CompileSession::StartAsync(const std::string& script_path) {
    if (wrapper_.IsRunning()) return;

    ClearLines();
    last_exit_code_.store(INT_MIN);

    std::thread([this, script_path]() {
        auto result = wrapper_.Invoke(script_path, [this](const CompilerLine& cl) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            queue_.push_back(cl);
        });
        last_exit_code_.store(result.exit_code);
    }).detach();
}

void CompileSession::Cancel() {
    wrapper_.Cancel();
}

void CompileSession::DrainQueue() {
    std::vector<CompilerLine> local;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        local.swap(queue_);
    }
    for (auto& l : local)
        display_lines_.push_back(std::move(l));
}

void CompileSession::ClearLines() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.clear();
    }
    display_lines_.clear();
}

} // namespace compiler
