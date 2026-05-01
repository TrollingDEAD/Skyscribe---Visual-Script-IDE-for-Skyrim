#include "ui/MainWindow.h"
#include "app/Logger.h"
#include "app/Settings.h"
#include "project/Project.h"
#include "compiler/CompileSession.h"

#include <imgui_internal.h>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#include <shellapi.h>


namespace ui {

// ── Singleton ─────────────────────────────────────────────────────────────────

MainWindow& MainWindow::Get() {
    static MainWindow instance;
    return instance;
}

// ── Render ────────────────────────────────────────────────────────────────────

void MainWindow::Render() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kHostFlags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus            |
        ImGuiWindowFlags_MenuBar               |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::Begin("##SkyscribeHost", nullptr, kHostFlags);
    ImGui::PopStyleVar(3);

    RenderMenuBar();

    // ── DockSpace ────────────────────────────────────────────────────────────
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        layout_built_ = false;

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (!layout_built_) {
        BuildDefaultLayout(dockspace_id);
        layout_built_ = true;
    }

    ImGui::End(); // ##SkyscribeHost

    // ── Drain logger queue ────────────────────────────────────────────────────
    app::Logger::Get().DrainQueue();

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
        if (project::Project::Get().IsOpen()) project::Project::Get().Save();
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) {
        // Open project picker — handled below via flag (can't call Win32 inside Begin)
        // For now just set a flag; TODO: wire picker properly
    }
    if (ImGui::IsKeyChordPressed(ImGuiKey_F7)) {
        // F7 = compile
        trigger_compile_ = true;
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Comma)) {
        settings_modal_.Open();
    }

    // ── Trigger deferred compile ──────────────────────────────────────────────
    if (trigger_compile_) {
        trigger_compile_ = false;
        const auto& s = app::Settings::Get();
        if (s.ck_compiler_path.empty() || !std::filesystem::exists(s.ck_compiler_path)) {
            show_no_ck_modal_ = true;
            ImGui::OpenPopup("NoCKFound");
        } else {
            // Phase 1: compile a test script if no project is open
            std::string script;
            if (project::Project::Get().IsOpen())
                script = project::Project::Get().Meta().root_dir + "\\main.psc";
            else
                script = "tests\\scripts\\HelloWorld.psc";
            compiler::CompileSession::Get().StartAsync(script);
        }
    }

    // ── Panels ───────────────────────────────────────────────────────────────
    tool_palette_.Render();
    graph_editor_.Render();
    preview_.Render();
    output_panel_.Render();

    // ── Modals ────────────────────────────────────────────────────────────────
    settings_modal_.Render();
    new_project_dialog_.Render();
    RenderNoCKModal();
    RenderUnsavedChangesModal();
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

void MainWindow::RenderMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    auto& proj = project::Project::Get();

    // ── File ─────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("File")) {

        if (ImGui::MenuItem("New Project…")) {
            if (proj.IsDirty()) {
                show_unsaved_changes_modal_ = true;
                pending_after_unsaved_ = 1;
                ImGui::OpenPopup("UnsavedChanges");
            } else {
                new_project_dialog_.Open();
            }
        }

        if (ImGui::MenuItem("Open Project…", "Ctrl+O")) {
            char buf[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Skyscribe Project\0*.skyscribe\0";
            ofn.lpstrFile   = buf;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&ofn))
                proj.Open(buf);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", "Ctrl+S", false, proj.IsOpen()))
            proj.Save();

        if (ImGui::MenuItem("Save As…", nullptr, false, proj.IsOpen())) {
            char buf[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Skyscribe Project\0*.skyscribe\0";
            ofn.lpstrFile   = buf;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrDefExt = "skyscribe";
            ofn.Flags       = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn))
                proj.SaveAs(buf);
        }

        if (ImGui::MenuItem("Close Project", nullptr, false, proj.IsOpen())) {
            if (proj.IsDirty()) {
                show_unsaved_changes_modal_ = true;
                pending_after_unsaved_ = 3;
                ImGui::OpenPopup("UnsavedChanges");
            } else {
                proj.ForceClose();
            }
        }

        ImGui::Separator();

        // Recent Projects submenu
        if (ImGui::BeginMenu("Recent Projects",
                             !proj.RecentProjects().empty())) {
            for (const auto& p : proj.RecentProjects()) {
                if (ImGui::MenuItem(p.c_str()))
                    proj.Open(p);
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            PostQuitMessage(0);

        ImGui::EndMenu();
    }

    // ── Edit ─────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Settings…", "Ctrl+,"))
            settings_modal_.Open();
        ImGui::EndMenu();
    }

    // ── Compile ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Compile")) {
        if (ImGui::MenuItem("Build", "F7",
                            false, !compiler::CompileSession::Get().IsRunning()))
            trigger_compile_ = true;

        if (ImGui::MenuItem("Cancel", nullptr,
                            false, compiler::CompileSession::Get().IsRunning()))
            compiler::CompileSession::Get().Cancel();

        ImGui::EndMenu();
    }

    // ── Help ─────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("View Log File…"))
            app::Logger::Get().OpenLogFile();
        if (ImGui::MenuItem("Report a Bug…"))
            ShellExecuteW(nullptr, L"open",
                L"https://github.com/TrollingDEAD/Skyscribe---Visual-Script-IDE-for-Skyrim/issues",
                nullptr, nullptr, SW_SHOW);
        ImGui::Separator();
        if (ImGui::MenuItem("About Skyscribe")) { /* stub — Phase 1.10 */ }
        ImGui::EndMenu();
    }

    // ── Title bar project indicator ───────────────────────────────────────────
    if (proj.IsOpen()) {
        const std::string title = "  |  " + proj.Meta().name +
                                  (proj.IsDirty() ? " *" : "");
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 
                             ImGui::CalcTextSize(title.c_str()).x - 4.0f);
        ImGui::TextDisabled("%s", title.c_str());
    }

    ImGui::EndMenuBar();
}

// ── No-CK modal ───────────────────────────────────────────────────────────────

void MainWindow::RenderNoCKModal() {
    if (!show_no_ck_modal_) return;

    if (ImGui::BeginPopupModal("NoCKFound", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "Creation Kit compiler not found.\n"
            "Please configure the path in Edit \xe2\x86\x92 Settings.");
        ImGui::Spacing();

        if (ImGui::Button("Open Settings", ImVec2(130, 0))) {
            settings_modal_.Open();
            ImGui::CloseCurrentPopup();
            show_no_ck_modal_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Download CK", ImVec2(130, 0))) {
            ShellExecuteW(nullptr, L"open",
                L"https://store.steampowered.com/app/1946180",
                nullptr, nullptr, SW_SHOW);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
            show_no_ck_modal_ = false;
        }
        ImGui::EndPopup();
    }
}

// ── Unsaved changes modal ─────────────────────────────────────────────────────

void MainWindow::RenderUnsavedChangesModal() {
    if (!show_unsaved_changes_modal_) return;

    if (ImGui::BeginPopupModal("UnsavedChanges", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("You have unsaved changes. Save before continuing?");
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(80, 0))) {
            project::Project::Get().Save();
            ImGui::CloseCurrentPopup();
            show_unsaved_changes_modal_ = false;
            if (pending_after_unsaved_ == 1) new_project_dialog_.Open();
            else if (pending_after_unsaved_ == 3) project::Project::Get().ForceClose();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
            show_unsaved_changes_modal_ = false;
            if (pending_after_unsaved_ == 1) new_project_dialog_.Open();
            else if (pending_after_unsaved_ == 3) project::Project::Get().ForceClose();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            pending_after_unsaved_ = 0;
            ImGui::CloseCurrentPopup();
            show_unsaved_changes_modal_ = false;
        }
        ImGui::EndPopup();
    }
}

// ── Default layout ────────────────────────────────────────────────────────────
//
//  ┌──────────┬───────────────────────┬──────────────┐
//  │  Palette │    Graph Editor        │   Preview    │
//  │   ~20 %  │       ~55 %            │    ~25 %     │
//  └──────────┴───────────────────────┴──────────────┘
//  └─────────────────── Output ───────────────────────┘  (bottom, ~25% height)

void MainWindow::BuildDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_top, dock_output;
    // Split bottom 25% for Output panel
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f,
                                &dock_output, &dock_top);

    ImGuiID dock_remaining = dock_top;
    ImGuiID dock_palette, dock_preview, dock_graph;

    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Left, 0.20f,
                                &dock_palette, &dock_remaining);
    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Right, 0.3125f,
                                &dock_preview, &dock_graph);

    ImGui::DockBuilderDockWindow("Tool Palette",  dock_palette);
    ImGui::DockBuilderDockWindow("Graph Editor",  dock_graph);
    ImGui::DockBuilderDockWindow("Preview",       dock_preview);
    ImGui::DockBuilderDockWindow("Output",        dock_output);

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ui
