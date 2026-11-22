/**
 * slang - a simple scripting language.
 *
 * Diagnostic subsystem tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "compiler/diagnostics.h"

namespace
{

namespace diag = slang::compiler::diagnostics;

TEST(diagnostics, levenshtein_distance)
{
    EXPECT_EQ(diag::levenshtein_distance("", ""), 0);
    EXPECT_EQ(diag::levenshtein_distance("foo", "foo"), 0);
    EXPECT_EQ(diag::levenshtein_distance("foo", "foobar"), 3);
    EXPECT_EQ(diag::levenshtein_distance("kitten", "sitting"), 3);
    EXPECT_EQ(diag::levenshtein_distance("flaw", "lawn"), 2);
}

TEST(diagnostics, find_closest_match)
{
    std::vector<std::string> candidates = {"foo", "foobar", "bar", "baz", "quux"};

    auto match = diag::find_closest_match("fobar", candidates, 2);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(*match, "foobar");

    auto match2 = diag::find_closest_match("baq", candidates, 1);
    ASSERT_TRUE(match2.has_value());
    EXPECT_TRUE(*match2 == "bar" || *match2 == "baz");

    auto match3 = diag::find_closest_match("completely_different", candidates, 2);
    EXPECT_FALSE(match3.has_value());
}

TEST(diagnostics, file_line_lookup)
{
    diag::context ctx;
    const std::string filename = "test.sl";
    const std::string content = "line 1\nline 2\r\nline 3\nline 4";

    ctx.register_file(filename, content);

    EXPECT_TRUE(ctx.has_file(filename));
    EXPECT_FALSE(ctx.has_file("nonexistent.sl"));

    EXPECT_EQ(ctx.get_line(filename, 0), std::nullopt);
    EXPECT_EQ(ctx.get_line(filename, 1), "line 1");
    EXPECT_EQ(ctx.get_line(filename, 2), "line 2");
    EXPECT_EQ(ctx.get_line(filename, 3), "line 3");
    EXPECT_EQ(ctx.get_line(filename, 4), "line 4");
    EXPECT_EQ(ctx.get_line(filename, 5), std::nullopt);
}

TEST(diagnostics, format_style)
{
    diag::context ctx;
    const std::string filename = "example.sl";
    const std::string content =
      "fn test() -> i32\n"
      "{\n"
      "    i32 bar = 0;\n"
      "    bar = foo + 1;\n"
      "    return bar;\n"
      "}\n";

    ctx.register_file(filename, content);

    diag::diagnostic d{
      diag::severity::error,
      "unknown variable `foo`",
      filename,
      diag::source_span{slang::source_location{4, 11}, 3}};
    d.with_code("E0123")
      .with_label("unknown variable")
      .with_help("did you mean `foobar`?");

    ctx.report(std::move(d));

    EXPECT_TRUE(ctx.has_errors());
    EXPECT_EQ(ctx.get_error_count(), 1);
    EXPECT_EQ(ctx.get_warning_count(), 0);

    std::string formatted = ctx.format_all();

    const std::string expected =
      "error[E0123]: unknown variable `foo`\n"
      "  --> example.sl:4:11\n"
      "   |\n"
      " 4 |     bar = foo + 1;\n"
      "   |           ^^^ unknown variable\n"
      "   |\n"
      "help: did you mean `foobar`?\n";

    EXPECT_EQ(formatted, expected);
}

TEST(diagnostics, warnings_and_notes)
{
    diag::context ctx;
    const std::string filename = "warn.sl";
    const std::string content = "i32 unused_var = 42;\n";

    ctx.register_file(filename, content);

    auto& w = ctx.report_warning(
      filename,
      diag::source_span{slang::source_location{1, 5}, 10},
      "unused variable `unused_var`",
      "W0001");
    w.with_label("unused variable")
      .with_note("prefix with `_` to silence");

    EXPECT_FALSE(ctx.has_errors());
    EXPECT_TRUE(ctx.has_warnings());
    EXPECT_EQ(ctx.get_warning_count(), 1);

    std::string formatted = ctx.format_all();

    const std::string expected =
      "warning[W0001]: unused variable `unused_var`\n"
      "  --> warn.sl:1:5\n"
      "   |\n"
      " 1 | i32 unused_var = 42;\n"
      "   |     ^^^^^^^^^^ unused variable\n"
      "   |\n"
      "note: prefix with `_` to silence\n";

    EXPECT_EQ(formatted, expected);
}

TEST(diagnostics, multiple_errors_separated_by_blank_lines)
{
    diag::context ctx;
    const std::string filename = "multi.sl";
    const std::string content =
      "fn test() -> i32\n"
      "{\n"
      "    let a: i32 = 1;\n"
      "    a();\n"
      "    bar();\n"
      "}\n";

    ctx.register_file(filename, content);

    auto& d1 = ctx.report_error(
      filename,
      diag::source_span{slang::source_location{4, 5}, 1},
      "unknown function `a`",
      "E0001");
    d1.with_label("unknown function")
      .with_help("did you mean `ab`?");

    auto& d2 = ctx.report_error(
      filename,
      diag::source_span{slang::source_location{5, 5}, 3},
      "unknown function `bar`",
      "E0002");
    d2.with_label("unknown function")
      .with_help("did you mean `baz`?");

    std::string formatted = ctx.format_all();

    const std::string expected =
      "error[E0001]: unknown function `a`\n"
      "  --> multi.sl:4:5\n"
      "   |\n"
      " 4 |     a();\n"
      "   |     ^ unknown function\n"
      "   |\n"
      "help: did you mean `ab`?\n"
      "\n"
      "error[E0002]: unknown function `bar`\n"
      "  --> multi.sl:5:5\n"
      "   |\n"
      " 5 |     bar();\n"
      "   |     ^^^ unknown function\n"
      "   |\n"
      "help: did you mean `baz`?\n";

    EXPECT_EQ(formatted, expected);
}

}    // namespace
