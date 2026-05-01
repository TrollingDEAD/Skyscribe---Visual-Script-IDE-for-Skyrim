#include "compiler/CompilerWrapper.h"
#include "compiler/OutputParser.h"
#include "app/Settings.h"
#include "app/Logger.h"

#include <Windows.h>

#include <thread>
#include <sstream>

namespace compiler {

// ── BuildArgs ─────────────────────────────────────────────────────────────────

std::string CompilerWrapper::BuildArgs(const std::string& script_path) const {
    const auto& s = app::Settings::Get();

    if (s.ck_compiler_path.empty()) return {};

    // Flags file
    const std::string flags = s.FindFlagsFile();
    if (flags.empty()) {
        LOG_WARN("CompilerWrapper: TESV_Papyrus_Flags.flg not found in CK Data path");
    }

    // Import dirs joined with semicolons
    const auto import_dirs = s.ResolvedImportDirs();
    std::string import_arg;
    for (size_t i = 0; i < import_dirs.size(); ++i) {
        if (i > 0) import_arg += ';';
        import_arg += import_dirs[i];
    }

    // Output dir — fall back to script's own directory
    const std::string out_dir = s.output_dir.empty()
        ? script_path.substr(0, script_path.find_last_of("\\/"))
        : s.output_dir;

    // Compose: "compiler.exe" "script.psc" -f="flags" -i="imports" -o="outdir"
    std::ostringstream cmd;
    cmd << '"' << s.ck_compiler_path << '"'
        << " \"" << script_path << '"';

    if (!flags.empty())
        cmd << " \"-f=" << flags << '"';

    if (!import_arg.empty())
        cmd << " \"-i=" << import_arg << '"';

    cmd << " \"-o=" << out_dir << '"';

    return cmd.str();
}

// ── Invoke ────────────────────────────────────────────────────────────────────

CompileResult CompilerWrapper::Invoke(
    const std::string& script_path,
    std::function<void(const CompilerLine&)> line_callback)
{
    CompileResult result;
    running_.store(true);
    cancelled_.store(false);
    process_handle_ = nullptr;

    const std::string cmd_line = BuildArgs(script_path);
    if (cmd_line.empty()) {
        LOG_ERR("CompilerWrapper::Invoke: cannot build command line (settings incomplete)");
        running_.store(false);
        result.exit_code = -2;
        return result;
    }

    LOG_INFO("CompilerWrapper: " + cmd_line);

    // ── Set up anonymous pipe for stdout/stderr ────────────────────────────
    HANDLE pipe_read  = nullptr;
    HANDLE pipe_write = nullptr;
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0)) {
        LOG_ERR("CompilerWrapper: CreatePipe failed");
        running_.store(false);
        return result;
    }
    // Don't let the read-end be inherited by the child.
    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    // ── Launch process ────────────────────────────────────────────────────
    STARTUPINFOW si = {};
    si.cb          = sizeof(STARTUPINFOW);
    si.hStdOutput  = pipe_write;
    si.hStdError   = pipe_write;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags     = STARTF_USESTDHANDLES;

    // CreateProcess needs a mutable wide buffer.
    std::wstring wcmd(cmd_line.begin(), cmd_line.end());

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(
        nullptr,
        wcmd.data(),
        nullptr, nullptr,
        TRUE,   // inherit handles
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    CloseHandle(pipe_write); // child has its own copy; close ours

    if (!ok) {
        LOG_ERR("CompilerWrapper: CreateProcess failed (GLE=" +
                std::to_string(GetLastError()) + ")");
        CloseHandle(pipe_read);
        running_.store(false);
        return result;
    }

    process_handle_ = pi.hProcess;
    CloseHandle(pi.hThread);

    // ── Read stdout on this thread (blocking) ─────────────────────────────
    // The caller is already expected to call Invoke from a worker thread.
    char buf[4096];
    std::string line_buf;
    DWORD bytes_read = 0;

    while (ReadFile(pipe_read, buf, sizeof(buf) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        line_buf += buf;

        // Drain complete lines
        size_t pos;
        while ((pos = line_buf.find('\n')) != std::string::npos) {
            std::string raw = line_buf.substr(0, pos);
            // Strip carriage return
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            line_buf.erase(0, pos + 1);

            CompilerLine cl = OutputParser::Parse(raw);
            result.lines.push_back(cl);
            if (line_callback) line_callback(cl);
        }

        if (cancelled_.load()) break;
    }

    // Flush any remaining partial line
    if (!line_buf.empty()) {
        CompilerLine cl = OutputParser::Parse(line_buf);
        result.lines.push_back(cl);
        if (line_callback) line_callback(cl);
    }

    CloseHandle(pipe_read);

    // ── Wait for process with 60-second timeout ────────────────────────────
    constexpr DWORD kTimeoutMs = 60'000;
    DWORD wait_result = WaitForSingleObject(pi.hProcess, kTimeoutMs);

    if (wait_result == WAIT_TIMEOUT || cancelled_.load()) {
        LOG_WARN("CompilerWrapper: process timed out or was cancelled — terminating");
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        result.exit_code = -3;
    } else {
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        result.exit_code = static_cast<int>(exit_code);
    }

    CloseHandle(pi.hProcess);
    process_handle_ = nullptr;

    result.success = (result.exit_code == 0);
    running_.store(false);
    cancelled_.store(false);

    LOG_INFO("CompilerWrapper: finished, exit_code=" + std::to_string(result.exit_code));
    return result;
}

// ── Cancel ────────────────────────────────────────────────────────────────────

void CompilerWrapper::Cancel() {
    if (!running_.load()) return;
    cancelled_.store(true);
    if (process_handle_) {
        TerminateProcess(static_cast<HANDLE>(process_handle_), 1);
    }
}

} // namespace compiler
