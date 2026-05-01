#include "ui/PreviewPanel.h"

namespace ui {

void PreviewPanel::Render() {
    ImGui::Begin("Preview");
    ImGui::TextDisabled("Preview — Phase 3");
    ImGui::End();
}

} // namespace ui
