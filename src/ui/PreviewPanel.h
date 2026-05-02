#pragma once

#include <imgui.h>
#include <string>
#include <vector>

namespace codegen { struct LintDiagnostic; }

namespace ui {

class PreviewPanel {
public:
    void Render();

    // Supply generated Papyrus source text to display.
    void SetSource(const std::string& src);

    // Supply lint diagnostics to display in the status bar.
    void SetDiagnostics(const std::vector<codegen::LintDiagnostic>& diags);

private:
    std::string source_;
    int         error_count_   = 0;
    int         warning_count_ = 0;
};

} // namespace ui
