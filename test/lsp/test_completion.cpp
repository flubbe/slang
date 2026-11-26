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

#include <algorithm>
#include <string>
#include <vector>

#include "../../src/lsp/completion.h"
#include "../../src/lsp/navigation.h"

namespace
{

const slang::lsp::completion_item* find_item(
  const std::vector<slang::lsp::completion_item>& items,
  const std::string& label)
{
    auto it = std::ranges::find_if(
      items,
      [&label](const slang::lsp::completion_item& item)
      {
          return item.label == label;
      });
    return (it != items.end()) ? &*it : nullptr;
}

TEST(completion, returns_matching_symbols)
{
    std::string source =
      "const answer: i32 = 42;\n"
      "fn main() -> i32 { return ans; }\n";

    // Cursor right after "ans" on line 1 (0-based char 29).
    slang::lsp::lsp_position pos{.line = 1, .character = 29};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", source, pos);

    const auto* item = find_item(completions, "answer");
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->kind, "constant");
}

TEST(completion, does_not_crash_on_prefix_search)
{
    std::string source =
      "const a: i32 = 7;\n"
      "const b: i32 = 7;\n";

    slang::lsp::lsp_position pos{.line = 0, .character = 0};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", source, pos);
    EXPECT_FALSE(completions.empty());
}

TEST(completion, empty_source_returns_none)
{
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", "", {});
    EXPECT_TRUE(completions.empty());
}

TEST(completion, struct_member_after_dot_without_semicolon)
{
    std::string source =
      "struct Point {\n"
      "    x: i32,\n"
      "    y: f32\n"
      "};\n"
      "\n"
      "fn main() -> i32 {\n"
      "    let p: Point = Point { x: 1, y: 2.0 as f32 };\n"
      "    p.\n"
      "}\n";

    // Cursor right after "p." on line 7 (col 6).
    slang::lsp::lsp_position pos{.line = 7, .character = 6};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", source, pos);

    ASSERT_EQ(completions.size(), 2u);

    const auto* x = find_item(completions, "x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->kind, "field");
    ASSERT_TRUE(x->detail.has_value());
    EXPECT_EQ(*x->detail, "i32");

    const auto* y = find_item(completions, "y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->kind, "field");
    ASSERT_TRUE(y->detail.has_value());
    EXPECT_EQ(*y->detail, "f32");
}

TEST(completion, struct_member_with_partial_prefix)
{
    std::string source =
      "struct Rect {\n"
      "    width: i32,\n"
      "    height: i32,\n"
      "    weight: f32\n"
      "};\n"
      "\n"
      "fn main() -> i32 {\n"
      "    let r: Rect = Rect { width: 10, height: 20, weight: 1.0 as f32 };\n"
      "    return r.we;\n"
      "}\n";

    // Cursor right after "r.we" on line 8 (col 15).
    slang::lsp::lsp_position pos{.line = 8, .character = 15};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", source, pos);

    ASSERT_EQ(completions.size(), 1u);
    EXPECT_EQ(completions[0].label, "weight");
    EXPECT_EQ(completions[0].kind, "field");
    ASSERT_TRUE(completions[0].detail.has_value());
    EXPECT_EQ(*completions[0].detail, "f32");
}

TEST(completion, nested_struct_member_and_call_result)
{
    std::string source_nested =
      "struct Inner {\n"
      "    value: i32\n"
      "};\n"
      "struct Outer {\n"
      "    inner: Inner,\n"
      "    flag: i32\n"
      "};\n"
      "fn make_outer() -> Outer {\n"
      "    return Outer { inner: Inner { value: 42 }, flag: 1 };\n"
      "}\n"
      "fn main() -> i32 {\n"
      "    let o: Outer = make_outer();\n"
      "    return o.inner.\n"
      "}\n";

    // Line 12: "    return o.inner." -> col 19.
    const auto inner_items = slang::lsp::get_completions(
      "file:///test.sl", source_nested, {.line = 12, .character = 19});
    ASSERT_EQ(inner_items.size(), 1u);
    EXPECT_EQ(inner_items[0].label, "value");
    EXPECT_EQ(inner_items[0].detail, std::optional<std::string>{"i32"});

    std::string source_call =
      "struct Inner {\n"
      "    value: i32\n"
      "};\n"
      "struct Outer {\n"
      "    inner: Inner,\n"
      "    flag: i32\n"
      "};\n"
      "fn make_outer() -> Outer {\n"
      "    return Outer { inner: Inner { value: 42 }, flag: 1 };\n"
      "}\n"
      "fn main() -> i32 {\n"
      "    return make_outer().\n"
      "}\n";

    // Line 11: "    return make_outer()." -> col 24.
    const auto call_items = slang::lsp::get_completions(
      "file:///test.sl", source_call, {.line = 11, .character = 24});
    ASSERT_EQ(call_items.size(), 2u);
    EXPECT_NE(find_item(call_items, "inner"), nullptr);
    EXPECT_NE(find_item(call_items, "flag"), nullptr);
}

TEST(completion, array_length_member_completion)
{
    std::string source =
      "fn main(args: str[]) -> i32 {\n"
      "    return args.\n"
      "}\n";

    slang::lsp::lsp_position pos{.line = 1, .character = 16};
    const auto completions =
      slang::lsp::get_completions("file:///test.sl", source, pos);

    ASSERT_EQ(completions.size(), 1u);
    EXPECT_EQ(completions[0].label, "length");
    EXPECT_EQ(completions[0].kind, "field");
    EXPECT_EQ(completions[0].detail, std::optional<std::string>{"i32"});
}

TEST(completion, format_empty_result)
{
    std::vector<slang::lsp::completion_item> items;
    const std::string json =
      slang::lsp::format_completion_result(items);
    EXPECT_EQ(json, "[]");
}

TEST(completion, format_result_with_items_and_detail)
{
    std::vector<slang::lsp::completion_item> items;
    items.push_back(slang::lsp::completion_item{
      .label = "x",
      .kind = "field",
      .detail = "i32"});
    const std::string json =
      slang::lsp::format_completion_result(items);
    EXPECT_NE(json.find("\"label\":\"x\""), std::string::npos);
    EXPECT_NE(json.find("\"kind\":5"), std::string::npos);
    EXPECT_NE(json.find("\"detail\":\"i32\""), std::string::npos);
}

}    // namespace
