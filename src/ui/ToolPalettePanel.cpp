#include "ui/ToolPalettePanel.h"
#include "app/Settings.h"
#include "graph/NodeRegistry.h"
#include "graph/BuiltinNodes.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

namespace {

static const char* CategoryName(graph::NodeCategory cat) {
    switch (cat) {
    case graph::NodeCategory::Event:       return "Events";
    case graph::NodeCategory::ControlFlow: return "Control Flow";
    case graph::NodeCategory::Variable:    return "Variables";
    case graph::NodeCategory::Math:        return "Math & Logic";
    case graph::NodeCategory::Debug:       return "Debug";
    case graph::NodeCategory::Actor:       return "Actor";
    case graph::NodeCategory::Quest:       return "Quest / Story";
    case graph::NodeCategory::Utility:     return "Utility / Timer";
    case graph::NodeCategory::Custom:      return "Custom";
    default:                               return "Other";
    }
}

static ImVec4 CategoryHeaderColor(graph::NodeCategory cat) {
    switch (cat) {
    case graph::NodeCategory::Event:       return ImVec4(0.80f, 0.20f, 0.20f, 1.0f);
    case graph::NodeCategory::ControlFlow: return ImVec4(0.30f, 0.30f, 0.80f, 1.0f);
    case graph::NodeCategory::Variable:    return ImVec4(0.20f, 0.60f, 0.20f, 1.0f);
    case graph::NodeCategory::Math:        return ImVec4(0.40f, 0.40f, 0.90f, 1.0f);
    case graph::NodeCategory::Debug:       return ImVec4(0.90f, 0.60f, 0.10f, 1.0f);
    case graph::NodeCategory::Actor:       return ImVec4(0.60f, 0.30f, 0.85f, 1.0f);
    case graph::NodeCategory::Quest:       return ImVec4(0.90f, 0.80f, 0.20f, 1.0f);
    case graph::NodeCategory::Utility:     return ImVec4(0.30f, 0.70f, 0.70f, 1.0f);
    default:                               return ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    }
}

// Case-insensitive substring search
static bool ContainsCI(const std::string& haystack, const char* needle) {
    std::string h = haystack;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return std::tolower(c); });
    std::string n(needle);
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    return h.find(n) != std::string::npos;
}

static void DrawNodeEntry(const graph::NodeDefinition& def) {
    ImGui::PushID(def.type_id.c_str());

    ImGui::Selectable(def.display_name.c_str(), false,
                      ImGuiSelectableFlags_None, ImVec2(0, 0));

    // Tooltip
    if (ImGui::IsItemHovered() && !def.tooltip.empty())
        ImGui::SetTooltip("%s", def.tooltip.c_str());

    // Drag-drop source — payload is the null-terminated type_id
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("NODE_TYPE",
            def.type_id.c_str(),
            def.type_id.size() + 1);
        ImGui::TextUnformatted(def.display_name.c_str());
        ImGui::EndDragDropSource();
    }

    ImGui::PopID();
}

} // anonymous namespace

namespace ui {

void ToolPalettePanel::Render() {
    ImGui::Begin("Tool Palette");

    // ── Search bar ────────────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    bool search_changed = ImGui::InputText("##search", search_buf_, sizeof(search_buf_));
    (void)search_changed;

    const bool searching = search_buf_[0] != '\0';

    const auto& all_nodes = graph::NodeRegistry::Get().AllNodes();

    // ── Filtered / flat list ──────────────────────────────────────────────────
    if (searching) {
        std::vector<const graph::NodeDefinition*> matches;
        for (const auto& def : all_nodes)
            if (ContainsCI(def.display_name, search_buf_) ||
                ContainsCI(CategoryName(def.category), search_buf_) ||
                ContainsCI(def.source_script, search_buf_))
                matches.push_back(&def);

        if (matches.empty()) {
            ImGui::TextDisabled("No nodes match \"%s\"", search_buf_);
        } else {
            for (const auto* def : matches)
                DrawNodeEntry(*def);
        }
        ImGui::End();
        return;
    }

    // ── Categorised tree view ─────────────────────────────────────────────────
    auto& palette_state = app::Settings::Get().palette_category_expanded;
    graph::NodeCategory last_cat = static_cast<graph::NodeCategory>(-1);
    bool last_open = false;

    for (const auto& def : all_nodes) {
        if (def.category != last_cat) {
            if (last_open) { ImGui::TreePop(); last_open = false; }
            last_cat = def.category;

            const char* cat_name = CategoryName(def.category);
            // Read persisted state; default to open
            bool want_open = true;
            auto it = palette_state.find(cat_name);
            if (it != palette_state.end()) want_open = it->second;

            ImGui::SetNextTreeNodeOpen(want_open, ImGuiCond_Once);
            ImGui::PushStyleColor(ImGuiCol_Text, CategoryHeaderColor(def.category));
            last_open = ImGui::TreeNodeEx(cat_name, ImGuiTreeNodeFlags_None);
            ImGui::PopStyleColor();

            // Persist toggle in-memory (flushed to disk when Settings::Save() is called)
            palette_state[cat_name] = last_open;
        }
        if (last_open) {
            ImGui::Indent(8.0f);
            DrawNodeEntry(def);
            ImGui::Unindent(8.0f);
        }
    }
    if (last_open) ImGui::TreePop();

    ImGui::End();
}

} // namespace ui
