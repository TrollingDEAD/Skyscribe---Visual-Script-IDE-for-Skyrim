#pragma once

#include <string>

namespace ui {

// Modal dialog for File → New Project.
class NewProjectDialog {
public:
    void Open();
    void Render(); // call every frame
    bool IsOpen() const { return open_; }

private:
    std::string BrowseForFolder() const;

    bool open_ = false;

    char name_buf_[256]       = {};
    char parent_dir_buf_[512] = {};
    std::string error_msg_;
};

} // namespace ui
