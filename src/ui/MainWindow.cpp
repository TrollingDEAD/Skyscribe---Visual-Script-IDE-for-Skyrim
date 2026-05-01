#include "ui/MainWindow.h"
#include "app/Logger.h"

#include <imgui_internal.h> // DockBuilder API

namespace ui {

// ── Singleton ─────────────────────────────────────────────────────────────────

MainWindow& MainWindow::Get() {
    static MainWindow instance;
    return instance;
}

// ── Render ────────────────────────────────────────────────────────────────────

void MainWindow::Render() {
    // Make the host window cover the entire OS viewport.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus          |
        ImGuiWindowFlags_MenuBar             |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
    ImGui::Begin("##SkyscribeHost", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    // ── Menu bar ─────────────────────────────────────────────────────────────
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // Populated in Phase 1 (New / Open / Save / Export).
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("View Log File..."))
                app::Logger::Get().OpenLogFile();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // ── DockSpace ────────────────────────────────────────────────────────────
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // Detect layout corruption (e.g. after a bad imgui.ini).
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        layout_built_ = false;

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Build default layout on first frame (or after corruption reset).
    if (!layout_built_) {
        BuildDefaultLayout(dockspace_id);
        layout_built_ = true;
    }

    ImGui::End(); // ##SkyscribeHost

    // ── Drain logger queue and render panels ──────────────────────────────────
    app::Logger::Get().DrainQueue();

    tool_palette_.Render();
    graph_editor_.Render();
    preview_.Render();
}

// ── Default layout ────────────────────────────────────────────────────────────
//
//  ┌──────────┬────────────────────────┬──────────────┐
//  │  Palette │     Graph Editor        │   Preview    │
//  │   ~20 %  │       ~55 %             │    ~25 %     │
//  └──────────┴────────────────────────┴──────────────┘

void MainWindow::BuildDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_remaining = dockspace_id;
    ImGuiID dock_palette, dock_preview, dock_graph;

    // Split left 20 % → Tool Palette; remainder stays in dock_remaining.
    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Left, 0.20f,
                                &dock_palette, &dock_remaining);

    // Split remaining right 31.25 % → Preview (≈25 % of total); rest → Graph Editor.
    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Right, 0.3125f,
                                &dock_preview, &dock_graph);

    ImGui::DockBuilderDockWindow("Tool Palette",  dock_palette);
    ImGui::DockBuilderDockWindow("Graph Editor",  dock_graph);
    ImGui::DockBuilderDockWindow("Preview",       dock_preview);

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ui
