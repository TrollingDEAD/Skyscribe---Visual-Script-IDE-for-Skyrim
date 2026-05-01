#include "project/TemplateRegistry.h"
#include "app/Logger.h"

#include <Windows.h>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace project {

TemplateRegistry& TemplateRegistry::Get() {
    static TemplateRegistry instance;
    return instance;
}

std::string TemplateRegistry::TemplatesRoot() const {
    // Locate the templates directory beside the exe.
    char exe_path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    return fs::path(exe_path).parent_path().string() + "\\templates";
}

void TemplateRegistry::Scan() {
    templates_.clear();
    const std::string root = TemplatesRoot();

    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        LOG_WARN("TemplateRegistry: templates directory not found at: " + root);
        return;
    }

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;

        const std::string manifest = entry.path().string() + "\\template.json";
        if (!fs::exists(manifest)) continue;

        try {
            std::ifstream f(manifest);
            json j = json::parse(f);

            ProjectTemplate t;
            t.id          = entry.path().filename().string();
            t.name        = j.value("name",        t.id);
            t.description = j.value("description", std::string{});
            templates_.push_back(std::move(t));
        } catch (const std::exception& e) {
            LOG_WARN(std::string("TemplateRegistry: failed to parse ") + manifest + ": " + e.what());
        }
    }

    // Sort alphabetically by name so order is deterministic
    std::sort(templates_.begin(), templates_.end(),
              [](const ProjectTemplate& a, const ProjectTemplate& b) {
                  return a.name < b.name;
              });

    LOG_INFO("TemplateRegistry: found " + std::to_string(templates_.size()) + " templates");
}

bool TemplateRegistry::Apply(const ProjectTemplate& tmpl,
                             const std::string& dest_dir) const {
    const std::string src_dir = TemplatesRoot() + "\\" + tmpl.id;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(src_dir, ec)) {
        if (entry.is_directory()) continue;
        const std::string filename = entry.path().filename().string();
        if (filename == "template.json") continue; // don't copy the manifest

        const std::string dest = dest_dir + "\\" + filename;
        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            LOG_WARN("TemplateRegistry: failed to copy " + filename + ": " + ec.message());
            return false;
        }
    }
    return true;
}

} // namespace project
