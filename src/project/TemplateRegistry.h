#pragma once

#include <string>
#include <vector>

namespace project {

struct ProjectTemplate {
    std::string id;          // directory name under templates/
    std::string name;        // display name
    std::string description; // shown in New Project dialog
};

// Scans <exe_dir>\templates\ for subdirectories, each containing a
// template.json manifest. Results are cached after the first Scan().
class TemplateRegistry {
public:
    static TemplateRegistry& Get();

    // (Re-)scan the templates directory next to the running executable.
    // Called once on startup; safe to call again to pick up new templates.
    void Scan();

    const std::vector<ProjectTemplate>& Templates() const { return templates_; }

    // Copy all stub files from the template into dest_dir.
    // Returns true on success.
    bool Apply(const ProjectTemplate& tmpl, const std::string& dest_dir) const;

    // Path to the templates root directory (next to the .exe).
    std::string TemplatesRoot() const;

private:
    TemplateRegistry() = default;
    TemplateRegistry(const TemplateRegistry&) = delete;
    TemplateRegistry& operator=(const TemplateRegistry&) = delete;

    std::vector<ProjectTemplate> templates_;
};

} // namespace project
