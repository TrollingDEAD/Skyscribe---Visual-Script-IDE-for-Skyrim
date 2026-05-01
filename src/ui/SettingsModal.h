#pragma once

#include "app/Settings.h"

#include <string>
#include <vector>

namespace ui {

// Modal settings dialog opened via Edit → Settings… (Ctrl+,).
// Edits a *copy* of Settings; applies only on OK.
class SettingsModal {
public:
    // Open the modal (call once on the frame the user triggers it).
    void Open();

    // Call every frame — renders the modal when open, no-op otherwise.
    void Render();

    bool IsOpen() const { return open_; }

private:
    void RenderCompilerTab();
    void RenderPathsTab();
    void RenderAutosaveTab();

    // Win32 pickers
    std::string BrowseForFile(const std::string& filter_desc,
                              const std::string& filter_ext) const;
    std::string BrowseForFolder() const;

    bool open_ = false;

    // Working copy — discarded if the user hits Cancel.
    std::string              ck_compiler_path_;
    std::string              ck_data_path_;
    std::string              output_dir_;
    std::vector<std::string> import_dirs_;
    bool                     autosave_enabled_    = true;
    int                      autosave_interval_s_ = 300;

    // Validation errors for the working copy
    std::vector<app::ValidationError> validation_errors_;
    int  selected_import_dir_ = -1; // for remove/reorder
};

} // namespace ui
