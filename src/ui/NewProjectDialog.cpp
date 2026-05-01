#include "ui/NewProjectDialog.h"
#include "project/Project.h"
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
    memset(name_buf_, 0, sizeof(name_buf_));
    memset(parent_dir_buf_, 0, sizeof(parent_dir_buf_));
    error_msg_.clear();
    ImGui::OpenPopup("New Project");
}

void NewProjectDialog::Render() {
    if (!open_) return;

    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("New Project", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Project name:");
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputText("##name", name_buf_, sizeof(name_buf_));

    ImGui::Spacing();
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

    if (!error_msg_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", error_msg_.c_str());
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
