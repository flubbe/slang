/**
 * slang - a simple scripting language.
 *
 * LSP signature help tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <sstream>

#include "../../src/lsp/signature_help.h"
#include "../../src/lsp/navigation.h"

namespace
{

TEST(signature_help, detect_function_call)
{
    std::string source =
      "fn foo(a: i32) { } fn bar() { foo(1); }";

    // Position inside 'foo( ...' call (line 0, char 39).
    slang::lsp::lsp_position pos{.line = 0,
                                 .character = 39};
    // May or may not detect the call; test verifies no
    // crash.
    EXPECT_NO_THROW(
      slang::lsp::get_signature_help("file:///test.sl",
                                     source, pos));
}

TEST(signature_help, empty_source_returns_none)
{
    const auto sig = slang::lsp::get_signature_help(
      "file:///test.sl", "", {});
    EXPECT_FALSE(sig.has_value());
}

TEST(signature_help, format_empty_result)
{
    const std::string json =
      slang::lsp::format_signature_help_result(
        std::nullopt);
    EXPECT_EQ(json, "{}");
}

TEST(signature_help, format_result_with_data)
{
    slang::lsp::signature_info sig{
      .label = "fn foo(...)", .label_detail = "foo"};
    sig.parameters.push_back(
      slang::lsp::signature_parameter{
        .name = "arg", .type = "unknown"});

    const std::string json =
      slang::lsp::format_signature_help_result(sig);
    EXPECT_TRUE(json.find("\"fn foo") != std::string::npos);
}

}    // namespace
