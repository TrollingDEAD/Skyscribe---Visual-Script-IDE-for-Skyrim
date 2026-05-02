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
    // When in edit mode this stores the latest generated source but does
    // NOT overwrite the user's edits in the editor.
    void SetSource(const std::string& src);

    // Supply lint diagnostics to display in the status bar.
    void SetDiagnostics(const std::vector<codegen::LintDiagnostic>& diags);

    // Returns true when the user has toggled manual edit mode.
    bool IsEditMode() const { return edit_mode_; }

    // Returns the editor's current text (used by compile when in edit mode).
    std::string GetEditedText() const;

    // Resets edit mode and restores the last generated source.
    // Called automatically when the active script changes.
    void ResetEditMode();

private:
    TextEditor  editor_;
    bool        lang_set_           = false;
    bool        edit_mode_          = false;
    bool        has_manual_edits_   = false;
    bool        confirm_sync_open_  = false;
    std::string source_;            // last generated source (always up-to-date)
    int         error_count_   = 0;
    int         warning_count_ = 0;
};

} // namespace ui
