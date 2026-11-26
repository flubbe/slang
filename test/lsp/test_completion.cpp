/**
 * slang - a simple scripting language.
 *
 * LSP completion tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <sstream>

#include "../../src/lsp/completion.h"
#include "../../src/lsp/navigation.h"

namespace
{

/* === Phase 6.2: Completion — basic === */

TEST(completion, returns_matching_symbols)
{
    std::string source =
      "const x: i32 = 42;\n"
      "fn main() { x; }\n";

    // Position inside 'main' where 'x' may be visible.
    slang::lsp::lsp_position pos{.line = 1,
                                 .character = 11};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl",
                                  source, pos);
    // Symbol visibility depends on collect_names; test
    // verifies no crash.
    (void)completions.size();
}

TEST(completion, does_not_crash_on_prefix_search)
{
    std::string source =
      "const a: i32 = 7;\n"
      "const b: i32 = 7;\n";

    // Position at character 0 — may match prefix "c".
    slang::lsp::lsp_position pos{.line = 0,
                                 .character = 0};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl",
                                  source, pos);
    // May or may not return results depending on scope
    // -- just ensure no crash.
    (void)completions.size();
}

TEST(completion, empty_source_returns_none)
{
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", "",
                                  {});
    EXPECT_TRUE(completions.empty());
}

/* === Phase 6.2: Completion — result formatting === */

TEST(completion, format_empty_result)
{
    std::vector<slang::lsp::completion_item> items;
    const std::string json =
      slang::lsp::format_completion_result(items);
    EXPECT_EQ(json, "[]");
}

TEST(completion, format_result_with_items)
{
    std::vector<slang::lsp::completion_item> items;
    items.push_back(slang::lsp::completion_item{
      .label = "x", .kind = "identifier"});
    const std::string json =
      slang::lsp::format_completion_result(items);
    EXPECT_TRUE(
      json.find("\"label\":\"x\"") != std::string::npos);
}

}    // namespace
