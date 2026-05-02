#include "codegen/PapyrusLexer.h"

#include <cctype>
#include <cstring>
#include <algorithm>

namespace codegen {

// ---------------------------------------------------------------------------
// Fast hand-rolled tokenizer — avoids std::regex overhead.
// TextEditor calls mTokenize(in_begin, in_end, out_begin, out_end, color)
// and expects it to return true when a token was recognised, with out_begin/
// out_end pointing to that token's range and 'color' set to the palette index.
// ---------------------------------------------------------------------------

static bool PapyrusTokenizeString(const char* b, const char* e,
                                   const char*& ob, const char*& oe)
{
    const char* p = b;
    if (*p != '"') return false;
    ++p;
    while (p < e) {
        if (*p == '\\' && p + 1 < e) { p += 2; continue; }
        if (*p == '"') { ob = b; oe = p + 1; return true; }
        ++p;
    }
    return false;
}

static bool PapyrusTokenizeIdentifier(const char* b, const char* e,
                                       const char*& ob, const char*& oe)
{
    const char* p = b;
    if (!std::isalpha((unsigned char)*p) && *p != '_') return false;
    ++p;
    while (p < e && (std::isalnum((unsigned char)*p) || *p == '_'))
        ++p;
    ob = b; oe = p; return true;
}

static bool PapyrusTokenizeNumber(const char* b, const char* e,
                                   const char*& ob, const char*& oe)
{
    const char* p = b;
    // optional sign
    if (p < e && (*p == '+' || *p == '-')) {
        // only consume sign if followed by a digit or '.'
        if (p + 1 < e && (std::isdigit((unsigned char)p[1]) || p[1] == '.'))
            ++p;
        else
            return false;
    }
    if (p >= e) return false;
    if (!std::isdigit((unsigned char)*p) && *p != '.') return false;

    bool hasDigit = std::isdigit((unsigned char)*p) != 0;
    while (p < e && std::isdigit((unsigned char)*p)) { ++p; hasDigit = true; }
    if (p < e && *p == '.') {
        ++p;
        while (p < e && std::isdigit((unsigned char)*p)) ++p;
    }
    if (!hasDigit) return false;
    // optional float suffix
    if (p < e && (*p == 'f' || *p == 'F')) ++p;
    ob = b; oe = p; return true;
}

static bool PapyrusTokenizePunctuation(const char* b, const char* /*e*/,
                                        const char*& ob, const char*& oe)
{
    switch (*b) {
    case '[': case ']': case '(': case ')': case '{': case '}':
    case '!': case '%': case '^': case '&': case '*': case '-':
    case '+': case '=': case '~': case '|': case '<': case '>':
    case '?': case '/': case ',': case '.': case ':':
        ob = b; oe = b + 1; return true;
    default: return false;
    }
}

static bool PapyrusTokenize(const char* b, const char* e,
                              const char*& ob, const char*& oe,
                              TextEditor::PaletteIndex& color)
{
    color = TextEditor::PaletteIndex::Max;

    // skip whitespace
    while (b < e && std::isblank((unsigned char)*b)) ++b;
    if (b == e) {
        ob = oe = e;
        color = TextEditor::PaletteIndex::Default;
        return true;
    }

    if (PapyrusTokenizeString(b, e, ob, oe)) {
        color = TextEditor::PaletteIndex::String; return true;
    }
    if (PapyrusTokenizeIdentifier(b, e, ob, oe)) {
        color = TextEditor::PaletteIndex::Identifier; return true;
    }
    if (PapyrusTokenizeNumber(b, e, ob, oe)) {
        color = TextEditor::PaletteIndex::Number; return true;
    }
    if (PapyrusTokenizePunctuation(b, e, ob, oe)) {
        color = TextEditor::PaletteIndex::Punctuation; return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

TextEditor::LanguageDefinition PapyrusLexer::GetLanguageDefinition()
{
    TextEditor::LanguageDefinition lang;
    lang.mName            = "Papyrus";
    lang.mCaseSensitive   = false;   // Papyrus is case-insensitive
    lang.mSingleLineComment = ";";
    lang.mCommentStart    = "";      // Papyrus has no block comments
    lang.mCommentEnd      = "";
    lang.mAutoIndentation = true;
    lang.mPreprocChar     = '\0';    // no preprocessor directive character
    lang.mTokenize        = PapyrusTokenize;

    // Keywords — stored UPPERCASE because TextEditor converts the matched
    // identifier to upper-case before the keyword lookup when mCaseSensitive
    // is false.
    static const char* const kKeywords[] = {
        "SCRIPTNAME", "EXTENDS", "IMPORT",
        "FUNCTION", "ENDFUNCTION",
        "EVENT", "ENDEVENT",
        "WHILE", "ENDWHILE",
        "IF", "ELSEIF", "ELSE", "ENDIF",
        "RETURN",
        "PROPERTY", "AUTO", "AUTOREADONLY",
        "GLOBAL", "NATIVE",
        "NEW", "AS", "IS",
        "NONE", "SELF", "PARENT",
        "INT", "FLOAT", "BOOL", "STRING", "VAR",
        "FORM", "OBJECTREFERENCE",
        "ACTOR", "QUEST", "WEAPON", "ARMOR",
        "SPELL", "MAGICEFFECT",
        "ACTIVEMAGICEFFECT",
        "TRUE", "FALSE",
        nullptr
    };
    for (int i = 0; kKeywords[i]; ++i)
        lang.mKeywords.insert(kKeywords[i]);

    return lang;
}

} // namespace codegen
