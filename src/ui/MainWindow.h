#pragma once

#include "ui/ToolPalettePanel.h"
#include "ui/GraphEditorPanel.h"
#include "ui/PreviewPanel.h"

#include <imgui.h>

namespace ui {

class MainWindow {
public:
    static MainWindow& Get();

    // Called once from Application::Init() when imgui.ini was successfully loaded.
    // Prevents DockBuilder from overwriting the restored layout.
    void SetLayoutAlreadyLoaded(bool v) { layout_built_ = v; }

    // Called every frame from Application::Run() after ImGui::NewFrame().
    void Render();

private:
    MainWindow() = default;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    void BuildDefaultLayout(ImGuiID dockspace_id);

    bool layout_built_ = false;

    ToolPalettePanel tool_palette_;
    GraphEditorPanel graph_editor_;
    PreviewPanel     preview_;
};

} // namespace ui
