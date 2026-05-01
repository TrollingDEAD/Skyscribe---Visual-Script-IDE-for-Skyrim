#include "project/Project.h"
#include "app/Logger.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace project {

// ── Singleton ─────────────────────────────────────────────────────────────────

Project& Project::Get() {
    static Project instance;
    return instance;
}

// ── New ───────────────────────────────────────────────────────────────────────

bool Project::New(const std::string& name, const std::string& parent_dir) {
    if (name.empty() || parent_dir.empty()) return false;

    const std::string root = parent_dir + "\\" + name;
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        LOG_ERR("Project::New: cannot create directory: " + ec.message());
        return false;
    }

    meta_.name               = name;
    meta_.root_dir           = root;
    meta_.created_at         = NowISO8601();
    meta_.skyscribe_version  = "0.1.0";
    is_open_                 = true;
    dirty_                   = false;

    if (!WriteFile(FilePath())) {
        ForceClose();
        return false;
    }

    AddToRecent(FilePath());
    LOG_INFO("Project created: " + FilePath());
    return true;
}

// ── Open ──────────────────────────────────────────────────────────────────────

bool Project::Open(const std::string& path) {
    // Tier 1: exact .skyscribe file
    std::string resolved;
    if (fs::is_regular_file(path) && fs::path(path).extension() == ".skyscribe") {
        resolved = path;
    } else if (fs::is_directory(path)) {
        // Tier 2: scan the directory for a .skyscribe file
        for (const auto& entry : fs::directory_iterator(path, std::error_code{})) {
            if (entry.path().extension() == ".skyscribe") {
                resolved = entry.path().string();
                break;
            }
        }
    }

    if (resolved.empty()) {
        LOG_WARN("Project::Open: no .skyscribe file found at: " + path);
        return false;
    }

    try {
        std::ifstream f(resolved);
        json j = json::parse(f);
        auto& m = j["meta"];
        meta_.name              = m.value("name",               std::string{});
        meta_.root_dir          = m.value("root_dir",           std::string{});
        meta_.created_at        = m.value("created_at",         std::string{});
        meta_.skyscribe_version = m.value("skyscribe_version",  std::string{});
    } catch (const std::exception& e) {
        LOG_ERR(std::string("Project::Open: parse failed: ") + e.what());
        return false;
    }

    is_open_ = true;
    dirty_   = false;
    AddToRecent(resolved);
    LOG_INFO("Project opened: " + resolved);
    return true;
}

// ── Save ──────────────────────────────────────────────────────────────────────

bool Project::Save() {
    if (!is_open_) return false;
    const bool ok = WriteFile(FilePath());
    if (ok) dirty_ = false;
    return ok;
}

bool Project::SaveAs(const std::string& new_path) {
    if (!is_open_) return false;

    // Derive new root dir from the chosen .skyscribe path.
    meta_.root_dir = fs::path(new_path).parent_path().string();

    const bool ok = WriteFile(new_path);
    if (ok) {
        dirty_ = false;
        AddToRecent(new_path);
    }
    return ok;
}

// ── Close ─────────────────────────────────────────────────────────────────────

bool Project::Close() {
    if (dirty_) return false; // caller must prompt the user
    ForceClose();
    return true;
}

void Project::ForceClose() {
    meta_    = {};
    is_open_ = false;
    dirty_   = false;
}

// ── FilePath ──────────────────────────────────────────────────────────────────

std::string Project::FilePath() const {
    if (!is_open_ || meta_.root_dir.empty() || meta_.name.empty()) return {};
    return meta_.root_dir + "\\" + meta_.name + ".skyscribe";
}

// ── Recent projects ───────────────────────────────────────────────────────────

void Project::AddToRecent(const std::string& path) {
    recent_.erase(std::remove(recent_.begin(), recent_.end(), path), recent_.end());
    recent_.insert(recent_.begin(), path);
    if (recent_.size() > 10)
        recent_.resize(10);
}

void Project::LoadRecent(const std::string& config_path) {
    if (!fs::exists(config_path)) return;
    try {
        std::ifstream f(config_path);
        json j = json::parse(f);
        if (j.contains("recent_projects") && j["recent_projects"].is_array())
            recent_ = j["recent_projects"].get<std::vector<std::string>>();
    } catch (...) {}
}

void Project::SaveRecent(const std::string& config_path) const {
    try {
        json j;
        std::ifstream fi(config_path);
        if (fi.is_open()) j = json::parse(fi);
        j["recent_projects"] = recent_;
        const std::string tmp = config_path + ".tmp";
        { std::ofstream fo(tmp); fo << j.dump(4); }
        fs::rename(tmp, config_path);
    } catch (const std::exception& e) {
        LOG_ERR(std::string("Project::SaveRecent failed: ") + e.what());
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

bool Project::WriteFile(const std::string& path) const {
    json j;
    j["meta"]["name"]               = meta_.name;
    j["meta"]["root_dir"]           = meta_.root_dir;
    j["meta"]["created_at"]         = meta_.created_at;
    j["meta"]["skyscribe_version"]  = meta_.skyscribe_version;
    j["graphs"]                     = json::array();
    j["settings_override"]          = json::object();

    const std::string tmp = path + ".tmp";
    try {
        { std::ofstream f(tmp); f << j.dump(4); }
        fs::rename(tmp, path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERR(std::string("Project::WriteFile failed: ") + e.what());
        std::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
}

std::string Project::NowISO8601() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t   = system_clock::to_time_t(now);
    struct tm tm_buf = {};
    localtime_s(&tm_buf, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

} // namespace project
