#pragma once

#include "TextEditor.h"

namespace codegen {

// Returns a TextEditor::LanguageDefinition configured for the Papyrus scripting
// language (Skyrim/Fallout 4).  Used to wire syntax highlighting into the
// ImGuiColorTextEdit preview widget.
class PapyrusLexer {
public:
    static TextEditor::LanguageDefinition GetLanguageDefinition();
};

} // namespace codegen
