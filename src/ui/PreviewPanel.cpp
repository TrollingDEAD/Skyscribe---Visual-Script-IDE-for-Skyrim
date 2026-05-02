#include "ui/PreviewPanel.h"
#include "codegen/LintPass.h"
#include "codegen/PapyrusLexer.h"

#include <imgui.h>
#include <string>

namespace ui {

PreviewPanel::PreviewPanel()
{
    editor_.SetReadOnly(true);
    editor_.SetPalette(TextEditor::GetDarkPalette());
}

void PreviewPanel::SetSource(const std::string& src) {
    source_ = src;  // always track the latest generated source
    if (!lang_set_) {
        editor_.SetLanguageDefinition(codegen::PapyrusLexer::GetLanguageDefinition());
        lang_set_ = true;
    }
    // In edit mode, preserve the user's edits; do not overwrite.
    if (!edit_mode_) {
        editor_.SetText(src);
    }
}

std::string PreviewPanel::GetEditedText() const {
    return editor_.GetText();
}

void PreviewPanel::ResetEditMode() {
    edit_mode_         = false;
    has_manual_edits_  = false;
    confirm_sync_open_ = false;
    editor_.SetReadOnly(true);
    // Restore the last generated source into the editor.
    editor_.SetText(source_);
}

void PreviewPanel::SetDiagnostics(const std::vector<codegen::LintDiagnostic>& diags) {
    error_count_   = 0;
    warning_count_ = 0;
    for (const auto& d : diags) {
        if (d.severity == codegen::LintSeverity::Error)   ++error_count_;
        if (d.severity == codegen::LintSeverity::Warning) ++warning_count_;
    }
}

void PreviewPanel::Render() {
    ImGui::Begin("Preview");

    // ── Edit-mode toolbar ──────────────────────────────────────────────────
    if (!edit_mode_) {
        if (ImGui::SmallButton("Edit")) {
            edit_mode_        = true;
            has_manual_edits_ = false;
            editor_.SetReadOnly(false);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Switch to manual-edit mode.\nCodegen will no longer overwrite the preview.");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.0f, 1.0f));
        if (ImGui::SmallButton("Read-only")) {
            ResetEditMode();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Return to read-only mode and restore generated source.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Sync from graph")) {
            if (has_manual_edits_) {
                confirm_sync_open_ = true;
                ImGui::OpenPopup("##SyncConfirm");
            } else {
                // No edits — sync immediately.
                editor_.SetText(source_);
                has_manual_edits_ = false;
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Replace editor content with the latest generated source.");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        ImGui::TextUnformatted("[EDIT]");
        ImGui::PopStyleColor();
    }

    // ── Sync confirmation modal ────────────────────────────────────────────
    if (ImGui::BeginPopupModal("##SyncConfirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Overwrite your manual edits with the generated source?");
        ImGui::Separator();
        if (ImGui::Button("Sync", ImVec2(120, 0))) {
            editor_.SetText(source_);
            has_manual_edits_  = false;
            confirm_sync_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            confirm_sync_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Status bar ─────────────────────────────────────────────────────────
    if (!live_preview_enabled_) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
        ImGui::TextUnformatted("[Live preview disabled]");
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    if (error_count_ > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::Text("%d error(s)", error_count_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    if (warning_count_ > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("%d warning(s)", warning_count_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    if (error_count_ == 0 && warning_count_ == 0 && !source_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
        ImGui::Text("OK");
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::Separator();

    // ── Source text (syntax-highlighted via ImGuiColorTextEdit) ───────────
    if (source_.empty() && !edit_mode_) {
        ImGui::TextDisabled("(no source generated)");
    } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        editor_.Render("##preview", ImVec2(avail.x, avail.y - 4.0f), false);
        // Track whether the user made any edits in edit mode.
        if (edit_mode_ && editor_.IsTextChanged())
            has_manual_edits_ = true;
    }

    ImGui::End();
}

} // namespace ui
