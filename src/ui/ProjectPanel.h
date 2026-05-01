#pragma once

#include <imgui.h>
#include <string>

namespace ui {

class ProjectPanel {
public:
    void Render();

private:
    // New-script inline dialog state
    bool        show_new_script_dialog_ = false;
    char        new_name_buf_[129]      = "NewScript";
    char        new_extends_buf_[129]   = "ObjectReference";

    // Rename state
    int         rename_idx_  = -1;
    char        rename_buf_[129] = {};
};

} // namespace ui
