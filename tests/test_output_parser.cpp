#include <catch2/catch_test_macros.hpp>
#include "compiler/OutputParser.h"

using namespace compiler;

TEST_CASE("OutputParser — error line", "[parser]") {
    const std::string raw =
        R"(C:\CK\Data\Scripts\Source\HelloWorld.psc(12,5): error: unknown identifier 'Foo')";
    CompilerLine line = OutputParser::Parse(raw);

    REQUIRE(line.kind        == LineKind::Error);
    REQUIRE(line.file        == R"(C:\CK\Data\Scripts\Source\HelloWorld.psc)");
    REQUIRE(line.line_number == 12);
    REQUIRE(line.text        == raw);
}

TEST_CASE("OutputParser — warning line", "[parser]") {
    const std::string raw =
        R"(C:\CK\Data\Scripts\Source\MyScript.psc(3,1): warning: variable 'x' is unused)";
    CompilerLine line = OutputParser::Parse(raw);

    REQUIRE(line.kind        == LineKind::Warning);
    REQUIRE(line.file        == R"(C:\CK\Data\Scripts\Source\MyScript.psc)");
    REQUIRE(line.line_number == 3);
}

TEST_CASE("OutputParser — success line", "[parser]") {
    const std::string raw = "Assembly of HelloWorld succeeded";
    CompilerLine line = OutputParser::Parse(raw);

    REQUIRE(line.kind == LineKind::Success);
    REQUIRE(line.file.empty());
    REQUIRE(line.line_number == 0);
}

TEST_CASE("OutputParser — info / unknown line", "[parser]") {
    const std::string raw = "Starting Papyrus Compiler v1.9";
    CompilerLine line = OutputParser::Parse(raw);

    REQUIRE(line.kind == LineKind::Info);
    REQUIRE(line.text == raw);
}

TEST_CASE("OutputParser — empty line is Info", "[parser]") {
    CompilerLine line = OutputParser::Parse("");
    REQUIRE(line.kind == LineKind::Info);
}

TEST_CASE("OutputParser — case-insensitive error keyword", "[parser]") {
    const std::string raw =
        R"(Script.psc(7,2): ERROR: something bad happened)";
    CompilerLine line = OutputParser::Parse(raw);
    REQUIRE(line.kind == LineKind::Error);
    REQUIRE(line.line_number == 7);
}
