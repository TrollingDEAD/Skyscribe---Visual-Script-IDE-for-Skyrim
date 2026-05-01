#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <fstream>

namespace app {

enum class LogLevel { Info, Warn, Error };

struct LogEntry {
    LogLevel    level;
    std::string timestamp; // "[HH:MM:SS]"
    std::string message;
};

class Logger {
public:
    static Logger& Get();

    void Init(const std::string& log_path);
    void Log(LogLevel level, const std::string& message);
    void Flush();

    // Opens skyscribe.log in the default text editor via ShellExecuteW.
    void OpenLogFile() const;

    // UI thread calls this each frame to drain queued entries into the display buffer.
    void DrainQueue();

    // Read-only access to the display buffer (already drained entries).
    const std::vector<LogEntry>& GetEntries() const { return display_entries_; }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::string CurrentTimestamp();
    static const char* LevelString(LogLevel level);

    std::string           log_path_;
    std::ofstream         log_file_;
    std::mutex            queue_mutex_;
    std::vector<LogEntry> queue_;          // written by any thread
    std::vector<LogEntry> display_entries_; // drained on UI thread each frame
};

} // namespace app

// ── Convenience macros ────────────────────────────────────────────────────────
#define LOG_INFO(msg) ::app::Logger::Get().Log(::app::LogLevel::Info,  (msg))
#define LOG_WARN(msg) ::app::Logger::Get().Log(::app::LogLevel::Warn,  (msg))
#define LOG_ERR(msg)  ::app::Logger::Get().Log(::app::LogLevel::Error, (msg))
