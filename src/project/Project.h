#pragma once

#include "graph/ScriptGraph.h"

#include <string>
#include <vector>

namespace project {

struct ProjectMeta {
    std::string name;
    std::string root_dir;
    std::string created_at;          // ISO-8601 timestamp
    std::string skyscribe_version;
};

// Manages the current open project.
// At most one project can be open at a time (accessed via Get()).
class Project {
public:
    static Project& Get();

    // ── Lifecycle ────────────────────────────────────────────────────────────

    // Create a new project directory + blank .skyscribe file.
    // Returns false on failure (e.g. dir already exists, I/O error).
    bool New(const std::string& name, const std::string& parent_dir);

    // Open project file at path (*.skyscribe).
    // Three-tier fallback: exact path → scan dir for *.skyscribe → return false.
    bool Open(const std::string& path);

    // Atomic save of the current project.
    bool Save();

    // Save under a new path. Updates root_dir.
    bool SaveAs(const std::string& new_skyscribe_path);

    // Unload the project. Checks dirty flag — if dirty, returns false so caller
    // can prompt the user; caller must handle the prompt, then call ForceClose().
    bool Close();
    void ForceClose();

    // ── State queries ────────────────────────────────────────────────────────

    bool IsOpen()   const { return is_open_; }
    bool IsDirty()  const { return dirty_;   }

    const ProjectMeta& Meta() const { return meta_; }

    // Returns the full path to the .skyscribe file.
    std::string FilePath() const;

    // Mark the project as having unsaved changes.
    void MarkDirty() { dirty_ = true; }

    // ── Scripts ──────────────────────────────────────────────────────────────

    // Add a new blank script. Returns a reference to it.
    graph::ScriptGraph& AddScript(const std::string& name,
                                  const std::string& extends = "ObjectReference");

    void RemoveScript(size_t index);
    void RenameScript(size_t index, const std::string& new_name);

    const std::vector<graph::ScriptGraph>& Scripts() const { return scripts_; }
          std::vector<graph::ScriptGraph>& Scripts()       { return scripts_; }

    int  ActiveScriptIndex() const { return active_script_idx_; }
    void SetActiveScript(int index);

    // ── Recent projects ──────────────────────────────────────────────────────

    const std::vector<std::string>& RecentProjects() const { return recent_; }
    void AddToRecent(const std::string& path);
    void LoadRecent(const std::string& config_path);
    void SaveRecent(const std::string& config_path) const;

private:
    Project() = default;
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;

    bool WriteFile(const std::string& path) const;
    static std::string NowISO8601();

    ProjectMeta meta_;
    bool        is_open_ = false;
    bool        dirty_   = false;

    std::vector<graph::ScriptGraph> scripts_;
    int                             active_script_idx_ = -1;

    std::vector<std::string> recent_; // up to 10, most-recent first
};

} // namespace project
