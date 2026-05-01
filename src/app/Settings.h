#pragma once

// Phase 0 stub — settings populated in Phase 1.

#include <string>

namespace app {

class Settings {
public:
    static Settings& Get();

    // Load settings from %APPDATA%\Skyscribe\settings.json. No-op if file absent.
    void Load(const std::string& path) {}

    // Persist current settings.
    void Save(const std::string& path) {}

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};

} // namespace app
