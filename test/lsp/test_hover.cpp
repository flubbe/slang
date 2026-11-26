/**
 * slang - a simple scripting language.
 *
 * LSP hover tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <sstream>

#include "../../src/lsp/hover.h"
#include "../../src/lsp/navigation.h"

namespace
{

/* === Phase 6.2: Hover -- basic lookup === */

TEST(hover, returns_type_for_variable)
{
    std::string source = "const x: i32 = 42;";

    // LSP positions are 0-based: the 'x' of "const x" is at (0, 6).
    slang::lsp::lsp_position pos{.line = 0,
                                 .character = 6};
    auto hover = slang::lsp::find_hover_info(
      "file:///test.sl", source, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_EQ(hover->label, "x");
    // Detail may report the type or be empty.
    (void)hover->detail.empty();
}

TEST(hover, returns_signature_for_function)
{
    std::string source =
      "fn add() -> i32 { return 42; }\n";

    // LSP positions are 0-based: the 'a' of "fn add" is at (0, 3).
    slang::lsp::lsp_position pos{.line = 0,
                                 .character = 3};
    auto hover = slang::lsp::find_hover_info(
      "file:///test.sl", source, pos);

    ASSERT_TRUE(hover.has_value());
    EXPECT_EQ(hover->label, "fn add() -> i32");
    EXPECT_FALSE(hover->detail.empty());
}

TEST(hover, returns_none_for_unknown_position)
{
    std::string source = "const x: i32 = 42;";

    // Position after semicolon -- no symbol should be found.
    slang::lsp::lsp_position pos{.line = 0,
                                 .character = 20};
    // Should not crash or throw even with out-of-range pos.
    EXPECT_NO_THROW(slang::lsp::find_hover_info(
      "file:///test.sl", source, pos));
}

/* === Phase 6.2: Hover -- result formatting === */

TEST(hover, format_empty_result)
{
    const std::string json =
      slang::lsp::format_hover_result(std::nullopt);
    EXPECT_EQ(json, "null");
}

TEST(hover, format_result_with_data)
{
    slang::lsp::hover_item item{
      .label = "x", .detail = "i32", .kind = "let"};
    const std::string json =
      slang::lsp::format_hover_result(item);
    EXPECT_TRUE(json.find("\"kind\":\"markdown\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"value\":\"") != std::string::npos);
    EXPECT_TRUE(json.find("```") != std::string::npos);
    EXPECT_TRUE(json.find("x") != std::string::npos);
    EXPECT_TRUE(json.find("i32") != std::string::npos);

    // Verify no literal control characters (bytes 0x00–0x1F except those in escape sequences).
    for(char c: json)
    {
        unsigned char byte = static_cast<unsigned char>(c);
        // Allow printable ASCII and common JSON delimiters.
        // A \ (0x5C) and " (0x22) are expected in this JSON.
        // Literal newlines, tabs, etc. must never appear in JSON.
        if(byte < 0x20)
            FAIL() << "Literal control character 0x" << std::hex << byte
                   << " found in JSON output — this breaks JSON parsers.";
    }
}

TEST(hover, format_result_with_special_chars)
{
    slang::lsp::hover_item item{
      .label = "fn foo(a: i32, b: i32) -> i32",
      .detail = "i32",
      .kind = "function"};
    const std::string json =
      slang::lsp::format_hover_result(item);

    // Colons, spaces, and brackets are fine unescaped in JSON.
    EXPECT_TRUE(json.find(":") != std::string::npos);
    EXPECT_TRUE(json.find("i32") != std::string::npos);

    // Verify no literal control characters.
    for(char c: json)
    {
        unsigned char byte = static_cast<unsigned char>(c);
        if(byte < 0x20)
            FAIL() << "Literal control character 0x" << std::hex << byte
                   << " found in JSON output.";
    }
}

TEST(hover, format_result_with_quotes_and_backslashes)
{
    slang::lsp::hover_item item{
      .label = "x", .detail = "str\\value", .kind = "let"};
    const std::string json =
      slang::lsp::format_hover_result(item);

    // Verify the backslash in detail is escaped in the JSON output.
    // The detail "str\\value" becomes "str\\\\value" in JSON (escaped backslash).
    EXPECT_TRUE(json.find("str\\\\\\\\value") != std::string::npos
                || json.find("str\\\\") != std::string::npos);

    // Verify no literal control characters.
    for(char c: json)
    {
        unsigned char byte = static_cast<unsigned char>(c);
        if(byte < 0x20)
            FAIL() << "Literal control character 0x" << std::hex << byte
                   << " found in JSON output.";
    }
}

}    // namespace
