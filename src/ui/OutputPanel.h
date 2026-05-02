#pragma once

#include <imgui.h>
#include "codegen/LintPass.h"

#include <vector>

namespace ui {

class OutputPanel {
public:
    void Render();

    void SetDiagnostics(const std::vector<codegen::LintDiagnostic>& diags);

private:
    void RenderFatalErrorModal();

    // 0 = All, 1 = Errors, 2 = Warnings, 3 = Info
    int  filter_index_    = 0;
    bool auto_scroll_     = true;
    bool show_fatal_modal_= false;

    std::vector<codegen::LintDiagnostic> lint_diags_;
};

} // namespace ui
