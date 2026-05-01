#pragma once

#include <string>
#include <vector>

namespace app {

struct ValidationError {
    std::string field;   // e.g. "ck_compiler_path"
    std::string message; // human-readable reason
};

class Settings {
public:
    static Settings& Get();

    // Compiler
    std::string ck_compiler_path; // Full path to PapyrusCompiler.exe
    std::string ck_data_path;     // CK Data\ directory
    std::string output_dir;       // .pex output directory

    // Paths
    std::vector<std::string> import_dirs; // extra import directories

    // Autosave
    bool autosave_enabled    = true;
    int  autosave_interval_s = 300; // default 5 minutes

    // Load from config_path. Missing keys take defaults. No-op if file absent.
    void Load(const std::string& config_path);

    // Atomic save to config_path (write .tmp then rename).
    void Save(const std::string& config_path) const;

    // Validate all fields. Returns an empty vector when everything is OK.
    std::vector<ValidationError> Validate() const;

    // Returns the path to the flags file inside ck_data_path, or "" if not found.
    std::string FindFlagsFile() const;

    // Returns deduplicated import directories (built-in Scripts\Source + user extras).
    std::vector<std::string> ResolvedImportDirs() const;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};

} // namespace app
