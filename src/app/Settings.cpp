#include "app/Settings.h"
#include "app/Logger.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace app {

Settings& Settings::Get() {
    static Settings instance;
    return instance;
}

// ── Load ──────────────────────────────────────────────────────────────────────

void Settings::Load(const std::string& config_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(config_path)) return;

    try {
        std::ifstream f(config_path);
        json j = json::parse(f);

        ck_compiler_path = j.value("ck_compiler_path", std::string{});
        ck_data_path     = j.value("ck_data_path",     std::string{});
        output_dir       = j.value("output_dir",       std::string{});

        if (j.contains("import_dirs") && j["import_dirs"].is_array()) {
            import_dirs = j["import_dirs"].get<std::vector<std::string>>();
        }

        autosave_enabled    = j.value("autosave_enabled",    true);
        autosave_interval_s = j.value("autosave_interval_s", 300);

        if (j.contains("window")) {
            const auto& w = j["window"];
            window_x = w.value("x", -1);
            window_y = w.value("y", -1);
            window_w = w.value("w", 1280);
            window_h = w.value("h", 720);
        }

        if (j.contains("ui") && j["ui"].contains("palette_state")) {
            const auto& ps = j["ui"]["palette_state"];
            for (auto it = ps.begin(); it != ps.end(); ++it)
                if (it.value().is_boolean())
                    palette_category_expanded[it.key()] = it.value().get<bool>();
        }
    } catch (const std::exception& e) {
        LOG_WARN(std::string("Settings::Load failed: ") + e.what());
    }
}

// ── Save ──────────────────────────────────────────────────────────────────────

void Settings::Save(const std::string& config_path) const {
    namespace fs = std::filesystem;

    json j;
    j["ck_compiler_path"]  = ck_compiler_path;
    j["ck_data_path"]      = ck_data_path;
    j["output_dir"]        = output_dir;
    j["import_dirs"]       = import_dirs;
    j["autosave_enabled"]  = autosave_enabled;
    j["autosave_interval_s"] = autosave_interval_s;
    j["window"] = {
        {"x", window_x}, {"y", window_y},
        {"w", window_w}, {"h", window_h}
    };

    {
        json ps = json::object();
        for (const auto& kv : palette_category_expanded)
            ps[kv.first] = kv.second;
        j["ui"]["palette_state"] = ps;
    }

    const std::string tmp_path = config_path + ".tmp";
    try {
        {
            std::ofstream f(tmp_path);
            f << j.dump(4);
        }
        fs::rename(tmp_path, config_path);
    } catch (const std::exception& e) {
        LOG_ERR(std::string("Settings::Save failed: ") + e.what());
        std::error_code ec;
        fs::remove(tmp_path, ec);
    }
}

// ── Validate ──────────────────────────────────────────────────────────────────

std::vector<ValidationError> Settings::Validate() const {
    namespace fs = std::filesystem;
    std::vector<ValidationError> errors;

    if (ck_compiler_path.empty() || !fs::exists(ck_compiler_path))
        errors.push_back({"ck_compiler_path", "PapyrusCompiler.exe not found"});

    if (ck_data_path.empty() || !fs::is_directory(ck_data_path))
        errors.push_back({"ck_data_path", "CK Data directory not found"});

    if (!output_dir.empty() && !fs::is_directory(output_dir))
        errors.push_back({"output_dir", "Output directory does not exist"});

    for (const auto& dir : import_dirs) {
        if (!fs::is_directory(dir))
            errors.push_back({"import_dirs", "Import directory not found: " + dir});
    }

    return errors;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string Settings::FindFlagsFile() const {
    namespace fs = std::filesystem;
    if (ck_data_path.empty()) return {};

    // Standard CK layout: Data\Scripts\Source\TESV_Papyrus_Flags.flg
    const std::string candidate =
        ck_data_path + "\\Scripts\\Source\\TESV_Papyrus_Flags.flg";
    if (fs::exists(candidate)) return candidate;

    // Fallback: scan recursively (slow — only done on demand)
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(ck_data_path, ec)) {
        if (!ec && entry.path().filename() == "TESV_Papyrus_Flags.flg")
            return entry.path().string();
    }
    return {};
}

std::vector<std::string> Settings::ResolvedImportDirs() const {
    namespace fs = std::filesystem;
    std::vector<std::string> dirs;

    // Built-in: Data\Scripts\Source
    if (!ck_data_path.empty()) {
        const std::string builtin = ck_data_path + "\\Scripts\\Source";
        if (fs::is_directory(builtin))
            dirs.push_back(builtin);
    }

    // User-specified extras
    for (const auto& d : import_dirs) {
        if (fs::is_directory(d)) {
            // Deduplicate
            bool found = false;
            for (const auto& existing : dirs) {
                std::error_code eq_ec;
                if (fs::equivalent(existing, d, eq_ec)) {
                    found = true; break;
                }
            }
            if (!found) dirs.push_back(d);
        }
    }

    return dirs;
}

} // namespace app
