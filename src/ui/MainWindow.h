#pragma once

#include "ui/ToolPalettePanel.h"
#include "ui/GraphEditorPanel.h"
#include "ui/ProjectPanel.h"
#include "ui/PreviewPanel.h"
#include "ui/OutputPanel.h"
#include "ui/SettingsModal.h"
#include "ui/NewProjectDialog.h"
#include "codegen/DirtyFlag.h"
#include "codegen/LintPass.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace ui {

class MainWindow {
public:
    static MainWindow& Get();

    void SetLayoutAlreadyLoaded(bool v) { layout_built_ = v; }

    // Called every frame from Application::Run() after ImGui::NewFrame().
    void Render();

private:
    MainWindow() = default;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    void BuildDefaultLayout(ImGuiID dockspace_id);
    void RenderMenuBar();
    void RenderNoCKModal();
    void RenderUnsavedChangesModal();
    void RenderAboutModal();
    void RenderShortcutsModal();

    bool layout_built_ = false;

    ToolPalettePanel  tool_palette_;
    GraphEditorPanel  graph_editor_;
    ProjectPanel      project_panel_;
    PreviewPanel      preview_;
    OutputPanel       output_panel_;
    SettingsModal     settings_modal_;
    NewProjectDialog  new_project_dialog_;

    // Compile trigger state
    bool trigger_compile_     = false;
    bool trigger_compile_all_ = false;

    // Lint diagnostics (task 3.14)
    std::vector<codegen::LintDiagnostic> last_lint_diags_;

    // Codegen state
    codegen::DirtyFlag codegen_dirty_; // starts dirty
    int  last_codegen_script_idx_ = -1;
    int  last_script_node_count_  = -1;
    int  last_prop_count_         = -1;
    std::string last_prop_script_;       // script name at last property sync

    // Cross-script node sync tracking (task 3.10)
    int  last_script_count_       = -1;
    int  last_cross_func_total_   = -1; // sum of function counts across all scripts

    // Modals
    bool show_no_ck_modal_           = false;
    bool show_unsaved_changes_modal_ = false;
    bool show_about_modal_           = false;
    bool show_shortcuts_modal_       = false;
    // What to do after the user resolves unsaved changes: 0=none,1=new,2=open,3=close
    int  pending_after_unsaved_      = 0;
    std::string pending_open_path_;
};

} // namespace ui
