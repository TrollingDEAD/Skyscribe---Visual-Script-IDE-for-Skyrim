#include "app/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace app {

// ── Singleton ─────────────────────────────────────────────────────────────────

Logger& Logger::Get() {
    static Logger instance;
    return instance;
}

// ── Init ──────────────────────────────────────────────────────────────────────

void Logger::Init(const std::string& log_path) {
    log_path_ = log_path;

    // Rotate log if it has grown over 10 MB.
    std::error_code ec;
    auto file_size = std::filesystem::file_size(log_path_, ec);
    if (!ec && file_size > 10 * 1024 * 1024) {
        std::filesystem::rename(log_path_, log_path_ + ".old", ec);
    }

    log_file_.open(log_path_, std::ios::app);
}

// ── Logging ───────────────────────────────────────────────────────────────────

void Logger::Log(LogLevel level, const std::string& message) {
    LogEntry entry;
    entry.level     = level;
    entry.timestamp = CurrentTimestamp();
    entry.message   = message;

    // Write to disk immediately (so crashes still produce a log).
    if (log_file_.is_open()) {
        log_file_ << entry.timestamp << " [" << LevelString(level) << "] "
                  << message << '\n';
        log_file_.flush();
    }

    // Queue for UI display.
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push_back(std::move(entry));
    }
}

void Logger::Flush() {
    if (log_file_.is_open())
        log_file_.flush();
}

// ── UI helpers ────────────────────────────────────────────────────────────────

void Logger::DrainQueue() {
    std::vector<LogEntry> local;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        local.swap(queue_);
    }
    for (auto& e : local)
        display_entries_.push_back(std::move(e));
}

void Logger::OpenLogFile() const {
    if (log_path_.empty()) return;
    std::wstring wide(log_path_.begin(), log_path_.end());
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOW);
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string Logger::CurrentTimestamp() {
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto t     = system_clock::to_time_t(now);
    struct tm tm_buf = {};
    localtime_s(&tm_buf, &t);

    std::ostringstream oss;
    oss << '[' << std::setfill('0')
        << std::setw(2) << tm_buf.tm_hour << ':'
        << std::setw(2) << tm_buf.tm_min  << ':'
        << std::setw(2) << tm_buf.tm_sec  << ']';
    return oss.str();
}

const char* Logger::LevelString(LogLevel level) {
    switch (level) {
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERR ";
    }
    return "????";
}

} // namespace app
