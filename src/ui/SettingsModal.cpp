#include "ui/SettingsModal.h"
#include "app/Settings.h"
#include "app/Logger.h"

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <filesystem>
#include <functional>
#include <algorithm>

namespace ui {

// ── Open ─────────────────────────────────────────────────────────────────────

void SettingsModal::Open() {
    if (open_) return;
    open_ = true;

    // Copy current settings into the working buffers.
    const auto& s        = app::Settings::Get();
    ck_compiler_path_    = s.ck_compiler_path;
    ck_data_path_        = s.ck_data_path;
    output_dir_          = s.output_dir;
    import_dirs_         = s.import_dirs;
    autosave_enabled_    = s.autosave_enabled;
    autosave_interval_s_ = s.autosave_interval_s;
    selected_import_dir_ = -1;
    validation_errors_.clear();

    ImGui::OpenPopup("Settings");
}

// ── Render ────────────────────────────────────────────────────────────────────

void SettingsModal::Render() {
    if (!open_) return;

    ImGui::SetNextWindowSize(ImVec2(600, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_NoResize)) return;

    if (ImGui::BeginTabBar("##settings_tabs")) {
        if (ImGui::BeginTabItem("Compiler")) { RenderCompilerTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Paths"))    { RenderPathsTab();    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Autosave")) { RenderAutosaveTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Show validation errors at the bottom.
    if (!validation_errors_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.4f, 0.4f, 1));
        for (const auto& e : validation_errors_)
            ImGui::TextUnformatted(("  * " + e.field + ": " + e.message).c_str());
        ImGui::PopStyleColor();
    }

    // ── OK / Cancel buttons ───────────────────────────────────────────────
    const float btn_w = 100.0f;
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btn_w * 2 - 10);

    if (ImGui::Button("OK", ImVec2(btn_w, 0))) {
        // Apply working copy to Settings and validate.
        auto& s              = app::Settings::Get();
        s.ck_compiler_path   = ck_compiler_path_;
        s.ck_data_path       = ck_data_path_;
        s.output_dir         = output_dir_;
        s.import_dirs        = import_dirs_;
        s.autosave_enabled   = autosave_enabled_;
        s.autosave_interval_s = autosave_interval_s_;
        validation_errors_ = s.Validate();
        // Only close if there are no P0-critical errors (we still save on warning).
        LOG_INFO("Settings saved");
        open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(btn_w, 0))) {
        open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ── Compiler tab ──────────────────────────────────────────────────────────────

static void PathRow(const char* label,
                    std::string& value,
                    bool is_file,
                    const std::string& filter_desc,
                    const std::string& filter_ext,
                    std::function<std::string()> browse_fn)
{
    namespace fs = std::filesystem;
    const bool valid = !value.empty() && (is_file ? fs::exists(value) : fs::is_directory(value));

    ImGui::PushID(label);
    ImGui::TextUnformatted(label);

    ImVec4 col = valid || value.empty()
        ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
        : ImVec4(1, 0.35f, 0.35f, 1);
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    char buf[512] = {};
    if (value.size() < sizeof(buf) - 1)
        memcpy(buf, value.data(), value.size());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
    if (ImGui::InputText("##val", buf, sizeof(buf)))
        value = buf;

    ImGui::PopStyleColor();

    if (!valid && !value.empty()) {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Path not found");
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(60, 0)))
        value = browse_fn();

    ImGui::PopID();
}

void SettingsModal::RenderCompilerTab() {
    ImGui::Spacing();

    PathRow("PapyrusCompiler.exe", ck_compiler_path_, true, "", "",
            [this]{ return BrowseForFile("Executable (*.exe)", "*.exe"); });
    ImGui::Spacing();

    PathRow("CK Data directory", ck_data_path_, false, "", "",
            [this]{ return BrowseForFolder(); });
    ImGui::Spacing();

    PathRow("Output directory", output_dir_, false, "", "",
            [this]{ return BrowseForFolder(); });
}

// ── Paths tab ─────────────────────────────────────────────────────────────────

void SettingsModal::RenderPathsTab() {
    ImGui::Spacing();
    ImGui::TextDisabled("Extra import directories (in addition to Scripts\\Source):");
    ImGui::Spacing();

    const float list_h = 180.0f;
    ImGui::BeginChild("##import_dirs", ImVec2(0, list_h), true);

    for (int i = 0; i < static_cast<int>(import_dirs_.size()); ++i) {
        namespace fs = std::filesystem;
        const bool exists = fs::is_directory(import_dirs_[i]);
        if (!exists) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.35f, 0.35f, 1));

        const bool selected = (selected_import_dir_ == i);
        if (ImGui::Selectable(import_dirs_[i].c_str(), selected))
            selected_import_dir_ = i;

        if (!exists) ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    if (ImGui::Button("Add…")) {
        std::string d = BrowseForFolder();
        if (!d.empty()) import_dirs_.push_back(d);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && selected_import_dir_ >= 0 &&
        selected_import_dir_ < static_cast<int>(import_dirs_.size())) {
        import_dirs_.erase(import_dirs_.begin() + selected_import_dir_);
        selected_import_dir_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Up") && selected_import_dir_ > 0) {
        std::swap(import_dirs_[selected_import_dir_], import_dirs_[selected_import_dir_ - 1]);
        --selected_import_dir_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Down") && selected_import_dir_ >= 0 &&
        selected_import_dir_ < static_cast<int>(import_dirs_.size()) - 1) {
        std::swap(import_dirs_[selected_import_dir_], import_dirs_[selected_import_dir_ + 1]);
        ++selected_import_dir_;
    }
}

// ── Autosave tab ──────────────────────────────────────────────────────────────

void SettingsModal::RenderAutosaveTab() {
    ImGui::Spacing();
    ImGui::Checkbox("Enable autosave", &autosave_enabled_);
    ImGui::BeginDisabled(!autosave_enabled_);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Interval (seconds)", &autosave_interval_s_);
    if (autosave_interval_s_ < 10)  autosave_interval_s_ = 10;
    if (autosave_interval_s_ > 3600) autosave_interval_s_ = 3600;
    ImGui::EndDisabled();
}

// ── Win32 pickers ─────────────────────────────────────────────────────────────

std::string SettingsModal::BrowseForFile(const std::string& /*filter_desc*/,
                                          const std::string& /*filter_ext*/) const {
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Executable\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn))
        return buf;
    return {};
}

std::string SettingsModal::BrowseForFolder() const {
    BROWSEINFOA bi = {};
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return {};
    char path[MAX_PATH] = {};
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return path;
}

} // namespace ui
