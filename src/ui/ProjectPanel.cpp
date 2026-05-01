#include "ui/ProjectPanel.h"
#include "project/Project.h"

#include <imgui.h>
#include <cstring>

namespace ui {

void ProjectPanel::Render() {
    ImGui::Begin("Project");

    auto& proj = project::Project::Get();

    if (!proj.IsOpen()) {
        ImGui::TextDisabled("No project open.");
        ImGui::End();
        return;
    }

    // ── Script list ───────────────────────────────────────────────────────────
    auto& scripts = proj.Scripts();
    int   active  = proj.ActiveScriptIndex();

    for (int i = 0; i < static_cast<int>(scripts.size()); ++i) {
        const auto& s = scripts[i];

        // Rename inline
        if (rename_idx_ == i) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 24.0f);
            if (ImGui::InputText("##rename", rename_buf_, sizeof(rename_buf_),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                proj.RenameScript(static_cast<size_t>(i), rename_buf_);
                rename_idx_ = -1;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemHovered())
                rename_idx_ = -1; // clicked away
            ImGui::SameLine();
            if (ImGui::SmallButton("OK")) {
                proj.RenameScript(static_cast<size_t>(i), rename_buf_);
                rename_idx_ = -1;
            }
            continue;
        }

        const bool is_active = (i == active);
        ImGui::PushID(i);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
        }

        if (ImGui::Selectable(s.script_name.c_str(), is_active,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            proj.SetActiveScript(i);
            if (ImGui::IsMouseDoubleClicked(0)) {
                // Double-click → rename
                rename_idx_ = i;
                std::strncpy(rename_buf_, s.script_name.c_str(), sizeof(rename_buf_) - 1);
                rename_buf_[sizeof(rename_buf_) - 1] = '\0';
            }
        }

        if (is_active) ImGui::PopStyleColor();

        // Extends hint
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", s.extends.c_str());

        // Delete button — hold Shift to confirm
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 22.0f);
        if (ImGui::SmallButton("X")) {
            if (ImGui::GetIO().KeyShift && scripts.size() > 1) {
                proj.RemoveScript(static_cast<size_t>(i));
                ImGui::PopID();
                break; // iterator invalidated
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hold Shift to delete");

        ImGui::PopID();
    }

    ImGui::Separator();

    // ── New script button ──────────────────────────────────────────────────────
    if (ImGui::Button("+ New Script")) {
        show_new_script_dialog_ = true;
        std::strncpy(new_name_buf_,    "NewScript",       sizeof(new_name_buf_) - 1);
        std::strncpy(new_extends_buf_, "ObjectReference", sizeof(new_extends_buf_) - 1);
    }

    if (show_new_script_dialog_) {
        ImGui::Separator();
        ImGui::TextUnformatted("Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##ns_name",    new_name_buf_,    sizeof(new_name_buf_));
        ImGui::TextUnformatted("Extends:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##ns_extends", new_extends_buf_, sizeof(new_extends_buf_));

        if (ImGui::Button("Create")) {
            if (new_name_buf_[0] != '\0') {
                proj.AddScript(new_name_buf_, new_extends_buf_);
                proj.SetActiveScript(static_cast<int>(proj.Scripts().size()) - 1);
                show_new_script_dialog_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            show_new_script_dialog_ = false;
    }

    ImGui::End();
}

} // namespace ui
