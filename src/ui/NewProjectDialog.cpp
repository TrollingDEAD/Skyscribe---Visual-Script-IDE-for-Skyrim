#include "ui/NewProjectDialog.h"
#include "project/Project.h"
#include "project/TemplateRegistry.h"
#include "app/Logger.h"

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shlobj.h>

#include <filesystem>
#include <cstring>

namespace ui {

void NewProjectDialog::Open() {
    if (open_) return;
    open_ = true;
    selected_template_ = 0;
    memset(name_buf_, 0, sizeof(name_buf_));
    memset(parent_dir_buf_, 0, sizeof(parent_dir_buf_));
    error_msg_.clear();
    ImGui::OpenPopup("New Project");
}

void NewProjectDialog::Render() {
    if (!open_) return;

    ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("New Project", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return;

    // ── Project name ──────────────────────────────────────────────────────────
    ImGui::TextUnformatted("Project name:");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##name", name_buf_, sizeof(name_buf_));

    ImGui::Spacing();

    // ── Parent directory ──────────────────────────────────────────────────────
    ImGui::TextUnformatted("Parent directory:");
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("##parent", parent_dir_buf_, sizeof(parent_dir_buf_));
    ImGui::SameLine();
    if (ImGui::Button("Browse")) {
        const std::string d = BrowseForFolder();
        if (!d.empty()) {
            strncpy_s(parent_dir_buf_, d.c_str(), sizeof(parent_dir_buf_) - 1);
        }
    }

    ImGui::Spacing();

    // ── Template picker ───────────────────────────────────────────────────────
    const auto& templates = project::TemplateRegistry::Get().Templates();
    if (!templates.empty()) {
        ImGui::TextUnformatted("Template:");
        ImGui::BeginChild("##templates", ImVec2(0, 100), true);

        for (int i = 0; i < static_cast<int>(templates.size()); ++i) {
            const bool selected = (selected_template_ == i);
            if (ImGui::Selectable(templates[i].name.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                selected_template_ = i;
            }
            if (selected && ImGui::IsItemHovered() &&
                !templates[i].description.empty()) {
                ImGui::SetTooltip("%s", templates[i].description.c_str());
            }
        }

        ImGui::EndChild();

        // Show description below the list
        if (selected_template_ < static_cast<int>(templates.size()) &&
            !templates[selected_template_].description.empty()) {
            ImGui::TextDisabled("%s", templates[selected_template_].description.c_str());
        }
        ImGui::Spacing();
    }

    // ── Error message ─────────────────────────────────────────────────────────
    if (!error_msg_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", error_msg_.c_str());
        ImGui::Spacing();
    }

    ImGui::Separator();

    if (ImGui::Button("Create", ImVec2(100, 0))) {
        const std::string name(name_buf_);
        const std::string parent(parent_dir_buf_);

        if (name.empty()) {
            error_msg_ = "Project name cannot be empty.";
        } else if (parent.empty() || !std::filesystem::is_directory(parent)) {
            error_msg_ = "Please choose a valid parent directory.";
        } else {
            if (project::Project::Get().New(name, parent)) {
                // Apply the selected template's stub files (if any)
                const auto& tmpls = project::TemplateRegistry::Get().Templates();
                if (!tmpls.empty() &&
                    selected_template_ < static_cast<int>(tmpls.size())) {
                    const std::string dest =
                        parent + "\\" + name + "\\Scripts\\Source";
                    std::error_code ec;
                    std::filesystem::create_directories(dest, ec);
                    project::TemplateRegistry::Get().Apply(
                        tmpls[selected_template_], dest);
                }
                open_ = false;
                ImGui::CloseCurrentPopup();
            } else {
                error_msg_ = "Failed to create project. Check the log for details.";
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        open_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

std::string NewProjectDialog::BrowseForFolder() const {
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
