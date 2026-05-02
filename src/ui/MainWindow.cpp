#include "ui/MainWindow.h"
#include "app/Logger.h"
#include "app/Settings.h"
#include "project/Project.h"
#include "compiler/CompileSession.h"
#include "codegen/PapyrusStringBuilder.h"
#include "codegen/LintPass.h"
#include "graph/BuiltinNodes.h"

#include <imgui_internal.h>
#include <filesystem>
#include <fstream>

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
        if (project::Project::Get().IsOpen()) {
            graph_editor_.SyncNodePositions();
            project::Project::Get().Save();
        }
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
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Slash)) {
        show_shortcuts_modal_ = true;
    }

    // ── Live codegen for preview ──────────────────────────────────────────────
    {
        auto& proj = project::Project::Get();
        if (proj.IsOpen() && !proj.Scripts().empty()) {
            int active = proj.ActiveScriptIndex();
            int node_count = (active >= 0 && active < (int)proj.Scripts().size())
                ? (int)proj.Scripts()[active].nodes.size() : -1;
            // Regenerate when: script switched, node count changed, or project dirtied.
            if (active != last_codegen_script_idx_ ||
                node_count != last_script_node_count_ ||
                proj.IsDirty() ||
                codegen_dirty_.IsSet())
            {
                codegen_dirty_.Clear();
                last_codegen_script_idx_ = active;
                last_script_node_count_  = node_count;

                if (active >= 0 && active < (int)proj.Scripts().size()) {
                    const auto& g = proj.Scripts()[active];

                    // ── Sync property nodes in registry (task 3.12) ────────────
                    int prop_count = (int)g.properties.size();
                    if (prop_count != last_prop_count_ ||
                        g.script_name != last_prop_script_) {
                        // If the script was renamed, clean up the old entries first
                        if (!last_prop_script_.empty() &&
                            last_prop_script_ != g.script_name) {
                            graph::BuiltinNodes::SyncPropertyNodes(last_prop_script_, {});
                        }
                        graph::BuiltinNodes::SyncPropertyNodes(g.script_name, g.properties);
                        last_prop_count_  = prop_count;
                        last_prop_script_ = g.script_name;
                    }

                    auto lint = codegen::LintPass::Run(g);
                    preview_.SetDiagnostics(lint);
                    auto gen = codegen::PapyrusStringBuilder::Generate(g);
                    preview_.SetSource(gen.source);
                }
            }
        }
    }

    // ── Trigger deferred compile ──────────────────────────────────────────────
    if (trigger_compile_) {
        trigger_compile_ = false;
        const auto& s = app::Settings::Get();
        if (s.ck_compiler_path.empty() || !std::filesystem::exists(s.ck_compiler_path)) {
            show_no_ck_modal_ = true;
            ImGui::OpenPopup("NoCKFound");
        } else {
            // Generate .psc source and write to Scripts\Source in the project dir.
            auto& proj2 = project::Project::Get();
            std::string script_path;
            if (proj2.IsOpen() && !proj2.Scripts().empty()) {
                int active = proj2.ActiveScriptIndex();
                if (active >= 0 && active < (int)proj2.Scripts().size()) {
                    const auto& g = proj2.Scripts()[active];
                    auto gen = codegen::PapyrusStringBuilder::Generate(g);
                    if (!gen.has_errors) {
                        std::string src_dir = proj2.Meta().root_dir + "\\Scripts\\Source";
                        std::filesystem::create_directories(src_dir);
                        script_path = src_dir + "\\" + g.script_name + ".psc";
                        std::ofstream f(script_path);
                        if (f) f << gen.source;
                    }
                }
            }
            if (script_path.empty())
                script_path = "tests\\scripts\\HelloWorld.psc";
            compiler::CompileSession::Get().StartAsync(script_path);
        }
    }

    // ── Panels ───────────────────────────────────────────────────────────────
    tool_palette_.Render();
    graph_editor_.Render();
    project_panel_.Render();
    preview_.Render();
    output_panel_.Render();

    // ── Modals ────────────────────────────────────────────────────────────────
    settings_modal_.Render();
    new_project_dialog_.Render();
    RenderNoCKModal();
    RenderUnsavedChangesModal();
    RenderAboutModal();
    RenderShortcutsModal();
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
        if (ImGui::MenuItem("Keyboard Shortcuts", "Ctrl+/"))
            show_shortcuts_modal_ = true;
        if (ImGui::MenuItem("View Log File…"))
            app::Logger::Get().OpenLogFile();
        if (ImGui::MenuItem("Report a Bug…"))
            ShellExecuteW(nullptr, L"open",
                L"https://github.com/TrollingDEAD/Skyscribe---Visual-Script-IDE-for-Skyrim/issues",
                nullptr, nullptr, SW_SHOW);
        ImGui::Separator();
        if (ImGui::MenuItem("About Skyscribe"))
            show_about_modal_ = true;
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
    ImGuiID dock_left, dock_preview, dock_graph;

    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Left, 0.20f,
                                &dock_left, &dock_remaining);
    ImGui::DockBuilderSplitNode(dock_remaining, ImGuiDir_Right, 0.3125f,
                                &dock_preview, &dock_graph);

    // Split left column: palette (top 50%) + project (bottom 50%)
    ImGuiID dock_palette, dock_project;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f,
                                &dock_project, &dock_palette);

    ImGui::DockBuilderDockWindow("Tool Palette",  dock_palette);
    ImGui::DockBuilderDockWindow("Project",       dock_project);
    ImGui::DockBuilderDockWindow("Graph Editor",  dock_graph);
    ImGui::DockBuilderDockWindow("Preview",       dock_preview);
    ImGui::DockBuilderDockWindow("Output",        dock_output);

    ImGui::DockBuilderFinish(dockspace_id);
}

// ── About modal ───────────────────────────────────────────────────────────────

void MainWindow::RenderAboutModal() {
    if (!show_about_modal_) return;
    ImGui::OpenPopup("About Skyscribe");
    show_about_modal_ = false;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("About Skyscribe", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Skyscribe");
        ImGui::SameLine();
        ImGui::TextDisabled("Phase 1 — Compiler Pipeline");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "A visual Papyrus scripting IDE for The Elder Scrolls V: Skyrim.\n"
            "Built with C++17, Dear ImGui, and Direct3D 11.");
        ImGui::Spacing();
        ImGui::TextDisabled("github.com/TrollingDEAD/Skyscribe");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 60.0f) * 0.5f +
                              ImGui::GetCursorPosX());
        if (ImGui::Button("Close", ImVec2(60, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ── Keyboard Shortcuts modal ──────────────────────────────────────────────────

void MainWindow::RenderShortcutsModal() {
    if (!show_shortcuts_modal_) return;
    ImGui::OpenPopup("Keyboard Shortcuts");
    show_shortcuts_modal_ = false;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Keyboard Shortcuts", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoResize)) {
        struct ShortcutEntry { const char* key; const char* action; };
        static const ShortcutEntry kShortcuts[] = {
            { "Ctrl+N",      "New Project"              },
            { "Ctrl+O",      "Open Project"             },
            { "Ctrl+S",      "Save Project"             },
            { "Ctrl+Shift+S","Save Project As…"         },
            { "F7",          "Compile (Build)"          },
            { "Ctrl+,",      "Open Settings"            },
            { "Ctrl+/",      "Keyboard Shortcuts"       },
        };

        if (ImGui::BeginTable("##shortcuts", 2,
                              ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_RowBg,
                              ImVec2(0, 0))) {
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("Action",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const auto& s : kShortcuts) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(s.key);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(s.action);
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 60.0f) * 0.5f +
                              ImGui::GetCursorPosX());
        if (ImGui::Button("Close", ImVec2(60, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace ui
