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
    source_ = src;
    if (!lang_set_) {
        editor_.SetLanguageDefinition(codegen::PapyrusLexer::GetLanguageDefinition());
        lang_set_ = true;
    }
    editor_.SetText(src);
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

    // ── Status bar ─────────────────────────────────────────────────────────
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
    if (source_.empty()) {
        ImGui::TextDisabled("(no source generated)");
    } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        editor_.Render("##preview", ImVec2(avail.x, avail.y - 4.0f), false);
    }

    ImGui::End();
}

} // namespace ui
