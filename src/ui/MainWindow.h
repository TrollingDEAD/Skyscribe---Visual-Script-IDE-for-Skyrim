#pragma once

#include "ui/ToolPalettePanel.h"
#include "ui/GraphEditorPanel.h"
#include "ui/PreviewPanel.h"
#include "ui/OutputPanel.h"
#include "ui/SettingsModal.h"
#include "ui/NewProjectDialog.h"

#include <imgui.h>

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

    bool layout_built_ = false;

    ToolPalettePanel  tool_palette_;
    GraphEditorPanel  graph_editor_;
    PreviewPanel      preview_;
    OutputPanel       output_panel_;
    SettingsModal     settings_modal_;
    NewProjectDialog  new_project_dialog_;

    // Compile trigger state
    bool trigger_compile_   = false;

    // Modals
    bool show_no_ck_modal_           = false;
    bool show_unsaved_changes_modal_ = false;
    // What to do after the user resolves unsaved changes: 0=none,1=new,2=open,3=close
    int  pending_after_unsaved_      = 0;
    std::string pending_open_path_;
};

} // namespace ui
