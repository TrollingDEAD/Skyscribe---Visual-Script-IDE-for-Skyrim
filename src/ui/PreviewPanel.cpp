#include "ui/PreviewPanel.h"
#include "codegen/LintPass.h"

#include <imgui.h>
#include <string>

namespace ui {

void PreviewPanel::SetSource(const std::string& src) {
    source_ = src;
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

    // ── Source text ────────────────────────────────────────────────────────
    if (source_.empty()) {
        ImGui::TextDisabled("(no source generated)");
    } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        // Reserve a few pixels at the bottom for the status line
        ImGui::InputTextMultiline(
            "##preview_src",
            const_cast<char*>(source_.c_str()),
            source_.size() + 1,
            ImVec2(avail.x, avail.y - 4.0f),
            ImGuiInputTextFlags_ReadOnly);
    }

    ImGui::End();
}

} // namespace ui
