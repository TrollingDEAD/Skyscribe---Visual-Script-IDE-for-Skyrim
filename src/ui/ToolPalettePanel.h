#pragma once

#include <imgui.h>

namespace ui {

class ToolPalettePanel {
public:
    void Render();

private:
    char search_buf_[256] = {};
};

} // namespace ui
