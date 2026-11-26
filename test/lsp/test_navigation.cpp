/**
 * slang - a simple scripting language.
 *
 * LSP navigation tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <print>
#include <sstream>

#include "../../src/lsp/navigation.h"

namespace
{

/* === Phase 6.1: Document symbols === */

TEST(navigation, extract_document_symbols)
{
    const std::string source =
        "fn add(a: i32, b: i32) -> i32 {\n"
        "    let sum: i32 = a + b;\n"
        "    return sum;\n"
        "}\n"
        "struct Point { x: i32, y: i32 };\n"
        "const MAX_COUNT: i32 = 999;\n";

    const auto symbols =
        slang::lsp::compute_document_symbols(source);
    EXPECT_EQ(symbols.size(), 3U);

    // 1. Function 'add'
    EXPECT_EQ(symbols[0].name, "add");
    EXPECT_EQ(symbols[0].kind,
              slang::lsp::symbol_kind::function);
    EXPECT_EQ(symbols[0].children.size(), 3U);
    EXPECT_EQ(symbols[0].children[0].name, "a");
    EXPECT_EQ(symbols[0].children[0].kind,
              slang::lsp::symbol_kind::variable);
    EXPECT_EQ(symbols[0].children[1].name, "b");
    EXPECT_EQ(symbols[0].children[1].kind,
              slang::lsp::symbol_kind::variable);
    EXPECT_EQ(symbols[0].children[2].name, "sum");
    EXPECT_EQ(symbols[0].children[2].kind,
              slang::lsp::symbol_kind::variable);

    // 2. Struct 'Point'
    EXPECT_EQ(symbols[1].name, "Point");
    EXPECT_EQ(symbols[1].kind,
              slang::lsp::symbol_kind::struct_);
    EXPECT_EQ(symbols[1].children.size(), 2U);
    EXPECT_EQ(symbols[1].children[0].name, "x");
    EXPECT_EQ(symbols[1].children[0].kind,
              slang::lsp::symbol_kind::field);
    EXPECT_EQ(symbols[1].children[1].name, "y");
    EXPECT_EQ(symbols[1].children[1].kind,
              slang::lsp::symbol_kind::field);

    // 3. Constant 'MAX_COUNT'
    EXPECT_EQ(symbols[2].name, "MAX_COUNT");
    EXPECT_EQ(symbols[2].kind,
              slang::lsp::symbol_kind::constant);

    // Verify JSON serialization
    const std::string json =
        slang::lsp::format_document_symbols_result(symbols);
    EXPECT_TRUE(json.find("\"name\":\"add\"") != std::string::npos);
    EXPECT_TRUE(
        json.find("\"name\":\"Point\"") != std::string::npos);
    EXPECT_TRUE(
        json.find("\"name\":\"sum\"") != std::string::npos);
}

/* === Phase 6.1: Go to definition === */

TEST(navigation, find_definition_basic)
{
    const std::string source =
        "fn helper(x: i32) -> i32 { return x * 2; }\n"
        "fn main() -> i32 { return helper(42); }\n";

    const std::string uri = "file:///workspace/test.sl";

    // Position on 'helper' call (line 1, char 28)
    slang::lsp::lsp_position call_pos{.line = 1,
                                      .character = 28};
    const auto def = slang::lsp::find_definition(
        uri, source, call_pos, {});

    EXPECT_TRUE(def.has_value());
    EXPECT_EQ(def->uri, uri);
    EXPECT_EQ(def->range.start.line, 0);
    EXPECT_EQ(def->range.start.character, 3);    // 'helper'
}

TEST(navigation, find_definition_no_match)
{
    const std::string source =
        "fn helper(x: i32) -> i32 { return x * 2; }\n"
        "fn main() -> i32 { return helper(42); }\n";

    // Position on whitespace
    slang::lsp::lsp_position pos{.line = 1, .character = 0};
    const auto result =
        slang::lsp::find_definition("file:///test.sl", source,
                                    pos, {});
    EXPECT_FALSE(result.has_value());
}

TEST(navigation, find_definition_cross_file)
{
    const std::string source =
        "fn main() -> i32 { return helper(42); }\n";

    // Position on 'helper' in the call
    slang::lsp::lsp_position pos{.line = 0, .character = 25};

    // Empty search paths — should not find in external files.
    const auto result =
        slang::lsp::find_definition("file:///test.sl", source,
                                    pos, {});
    EXPECT_FALSE(result.has_value());
}

/* === Phase 6.1: Find references === */

TEST(navigation, find_references_with_declaration)
{
    const std::string source =
        "fn compute(n: i32) -> i32 { return n + 1; }\n"
        "fn main() -> i32 {\n"
        "    let a: i32 = compute(10);\n"
        "    let b: i32 = compute(20);\n"
        "    return a + b;\n"
        "}\n";

    slang::lsp::lsp_position decl_pos{.line = 0,
                                       .character = 4};

    const auto refs = slang::lsp::find_references(
        "file:///workspace/test.sl", source, decl_pos,
        true);
    EXPECT_EQ(refs.size(), 3U);    // 1 def + 2 calls
}

TEST(navigation, find_references_without_declaration)
{
    const std::string source =
        "fn compute(n: i32) -> i32 { return n + 1; }\n"
        "fn main() -> i32 {\n"
        "    let a: i32 = compute(10);\n"
        "    let b: i32 = compute(20);\n"
        "    return a + b;\n"
        "}\n";

    slang::lsp::lsp_position decl_pos{.line = 0,
                                       .character = 4};

    const auto refs = slang::lsp::find_references(
        "file:///workspace/test.sl", source, decl_pos,
        false);
    EXPECT_EQ(refs.size(), 2U);    // 2 calls only
    for (const auto& r : refs) {
        EXPECT_NE(r.range.start.line, 0);
    }
}

TEST(navigation, reference_json_serialization)
{
    const std::string source =
        "fn compute(n: i32) -> i32 { return n + 1; }\n"
        "fn main() -> i32 { return compute(10); }\n";

    slang::lsp::lsp_position pos{.line = 0, .character = 4};
    const auto refs = slang::lsp::find_references(
        "file:///workspace/test.sl", source, pos, true);

    const std::string json =
        slang::lsp::format_references_result(refs);
    EXPECT_TRUE(json.find(
                    "file:///workspace/test.sl") !=
                std::string::npos);
}

/* === Phase 6.1: fullRange ⊇ selectionRange === */

TEST(navigation, document_symbols_full_range_contains_selection_range)
{
    // Reproduces the bug: "selectionRange must be contained in fullRange"
    // reported by the VS Code language client.
    // See: https://microsoft.github.io/language-serverSpecification/protocols/LSP/, section
    // DocumentSymbol, where fullRange must contain selectionRange.
    const std::string source =
        "fn add(a: i32, b: i32) -> i32 {\n"
        "    let sum: i32 = a + b;\n"
        "    return sum;\n"
        "}\n"
        "struct Point { x: i32, y: i32 };\n"
        "const MAX_COUNT: i32 = 999;\n";

    const auto symbols =
        slang::lsp::compute_document_symbols(source);
    EXPECT_EQ(symbols.size(), 3U);

    constexpr auto check = [&](const slang::lsp::document_symbol& sym)
    {
        // fullRange.start <= selectionRange.start
        EXPECT_LE(sym.range.start.line, sym.selection_range.start.line)
            << "fullRange.start before selectionRange.start for '" << sym.name << "'";
        if(sym.range.start.line == sym.selection_range.start.line)
        {
            EXPECT_LE(sym.range.start.character, sym.selection_range.start.character)
                << "fullRange.start before selectionRange.start for '" << sym.name << "'";
        }
        // selectionRange.end <= fullRange.end
        EXPECT_LE(sym.selection_range.end.line, sym.range.end.line)
            << "selectionRange.end after fullRange.end for '" << sym.name << "'";
        if(sym.selection_range.end.line == sym.range.end.line)
        {
            EXPECT_LE(sym.selection_range.end.character, sym.range.end.character)
                << "selectionRange.end after fullRange.end for '" << sym.name << "'";
        }
    };

    for(const auto& sym: symbols)
    {
        check(sym);
        for(const auto& child: sym.children)
        {
            check(child);
        }
    }
}

}    // namespace
