#pragma once

#include <imgui.h>
#include <string>
#include <vector>

#include "TextEditor.h"

namespace codegen { struct LintDiagnostic; }

namespace ui {

class PreviewPanel {
public:
    PreviewPanel();

    void Render();

    // Supply generated Papyrus source text to display.
    void SetSource(const std::string& src);

    // Supply lint diagnostics to display in the status bar.
    void SetDiagnostics(const std::vector<codegen::LintDiagnostic>& diags);

private:
    TextEditor  editor_;
    bool        lang_set_      = false;
    std::string source_;
    int         error_count_   = 0;
    int         warning_count_ = 0;
};

} // namespace ui
