#pragma once

#include <imgui.h>

namespace ui {

class OutputPanel {
public:
    void Render();

private:
    void RenderFatalErrorModal();

    // 0 = All, 1 = Errors, 2 = Warnings, 3 = Info
    int  filter_index_    = 0;
    bool auto_scroll_     = true;
    bool show_fatal_modal_= false;
};

} // namespace ui
