#include "ui/OutputPanel.h"
#include "compiler/CompileSession.h"
#include "compiler/CompilerLine.h"
#include "app/Logger.h"

#include <imgui.h>
#include <climits>

namespace ui {

static const char* kFilterLabels[] = { "All", "Errors", "Warnings", "Info" };

void OutputPanel::Render() {
    auto& session = compiler::CompileSession::Get();
    session.DrainQueue();

    // Detect newly finished compile with unexpected exit code.
    const int last_code = session.LastExitCode();
    if (!session.IsRunning() && last_code != INT_MIN &&
        last_code != 0 && last_code != 1 && !show_fatal_modal_) {
        show_fatal_modal_ = true;
        ImGui::OpenPopup("CompilerFatalError");
    }

    ImGui::Begin("Output");

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##filter", &filter_index_, kFilterLabels, 4);
    ImGui::SameLine();

    // Auto-scroll toggle (📌 is not in the default font; use a text label)
    if (ImGui::Button(auto_scroll_ ? "[Scroll: ON]" : "[Scroll: OFF]"))
        auto_scroll_ = !auto_scroll_;

    if (session.IsRunning()) {
        ImGui::SameLine();
        if (ImGui::Button("[Cancel]"))
            session.Cancel();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Compiling...");
    }

    ImGui::Separator();

    // ── Output lines ─────────────────────────────────────────────────────────
    ImGui::BeginChild("##output_scroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    const auto& lines = session.GetLines();
    for (const auto& cl : lines) {
        using compiler::LineKind;

        // Apply filter
        if (filter_index_ == 1 && cl.kind != LineKind::Error)    continue;
        if (filter_index_ == 2 && cl.kind != LineKind::Warning)  continue;
        if (filter_index_ == 3 && cl.kind != LineKind::Info)     continue;

        ImVec4 colour = ImGui::GetStyleColorVec4(ImGuiCol_Text); // default
        const char* prefix = "";

        switch (cl.kind) {
        case LineKind::Error:
            colour = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            prefix = "[ERROR] ";
            break;
        case LineKind::Warning:
            colour = ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
            prefix = "[WARN]  ";
            break;
        case LineKind::Success:
            colour = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            prefix = "[OK]    ";
            break;
        case LineKind::Info:
        default:
            break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, colour);

        if (cl.kind == LineKind::Error || cl.kind == LineKind::Warning) {
            // "filename(line): message" — make it selectable for future click-to-navigate
            const std::string display = prefix + cl.file + "(" +
                std::to_string(cl.line_number) + "): " + cl.text;

            if (ImGui::Selectable(display.c_str(), false)) {
                LOG_INFO("Navigate to: " + cl.file + " line " +
                         std::to_string(cl.line_number));
            }
        } else {
            ImGui::TextUnformatted((std::string(prefix) + cl.text).c_str());
        }

        ImGui::PopStyleColor();
    }

    if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();

    RenderFatalErrorModal();
}

void OutputPanel::RenderFatalErrorModal() {
    if (!show_fatal_modal_) return;

    if (ImGui::BeginPopupModal("CompilerFatalError", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
            "The compiler exited with an unexpected error.");
        ImGui::Text("Exit code: %d", compiler::CompileSession::Get().LastExitCode());
        ImGui::Text("Check the Output panel and skyscribe.log for details.");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            show_fatal_modal_ = false;
        }
        ImGui::EndPopup();
    }
}

} // namespace ui
