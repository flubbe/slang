
/**
 * slang - a simple scripting language.
 *
 * Tests for format! macro utilities.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "compiler/format_util.h"

namespace slang
{

void PrintTo(const source_location& loc, std::ostream* os)
{
    *os << std::format("({}, {})", loc.line, loc.col);
}

}    // namespace slang

namespace slang::format_util
{

namespace
{

class parse_format_string_test : public ::testing::Test
{
protected:
    static constexpr source_location test_location{1, 1};

    std::vector<placeholder> parse(const std::string& fmt)
    {
        return parse_format_string(fmt, test_location);
    }

    void expect_error(
      const std::string& fmt,
      source_location expected_location)
    {
        try
        {
            parse(fmt);
            FAIL() << "Expected format_error.";
        }
        catch(const format_error& e)
        {
            EXPECT_EQ(e.location, expected_location);
        }
    }
};

TEST_F(parse_format_string_test, empty_string)
{
    const auto result = parse("");

    EXPECT_TRUE(result.empty());
}

TEST_F(parse_format_string_test, plain_test)
{
    const auto result = parse("Hello, world!");

    EXPECT_TRUE(result.empty());
}

TEST_F(parse_format_string_test, single_placeholder)
{
    const auto result = parse("Hello, {}!");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].start, 7);
    EXPECT_EQ(result[0].end, 9);
    EXPECT_EQ(result[0].specifier, std::nullopt);
}

TEST_F(parse_format_string_test, multiple_placeholders)
{
    const auto result = parse("{} + {} = {}");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0].start, 0);
    EXPECT_EQ(result[0].end, 2);
    EXPECT_EQ(result[0].specifier, std::nullopt);

    EXPECT_EQ(result[1].start, 5);
    EXPECT_EQ(result[1].end, 7);
    EXPECT_EQ(result[1].specifier, std::nullopt);

    EXPECT_EQ(result[2].start, 10);
    EXPECT_EQ(result[2].end, 12);
    EXPECT_EQ(result[2].specifier, std::nullopt);
}

TEST_F(parse_format_string_test, integer_specifier)
{
    const auto result = parse("{:d}");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].start, 0);
    EXPECT_EQ(result[0].end, 4);
    EXPECT_EQ(result[0].specifier, 'd');
}

TEST_F(parse_format_string_test, float_specifier)
{
    const auto result = parse("{:f}");

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result[0].specifier.has_value());
    EXPECT_EQ(*result[0].specifier, 'f');
}

TEST_F(parse_format_string_test, string_specifier)
{
    const auto result = parse("{:s}");

    ASSERT_EQ(result.size(), 1);
    ASSERT_TRUE(result[0].specifier.has_value());
    EXPECT_EQ(*result[0].specifier, 's');
}

TEST_F(parse_format_string_test, escaped_opening_brace)
{
    const auto result = parse("{{");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].start, 0);
    EXPECT_EQ(result[0].end, 1);
    EXPECT_EQ(result[0].specifier, '{');
}

TEST_F(parse_format_string_test, escaped_closing_brace)
{
    const auto result = parse("}}");

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].start, 0);
    EXPECT_EQ(result[0].end, 1);
    EXPECT_EQ(result[0].specifier, '}');
}

TEST_F(parse_format_string_test, escaped_brace_around_placeholder)
{
    const auto result = parse("{{ {} }}");

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0].start, 0);
    EXPECT_EQ(result[0].end, 1);
    EXPECT_EQ(result[0].specifier, '{');

    EXPECT_EQ(result[1].start, 3);
    EXPECT_EQ(result[1].end, 5);
    EXPECT_EQ(result[1].specifier, std::nullopt);

    EXPECT_EQ(result[2].start, 6);
    EXPECT_EQ(result[2].end, 7);
    EXPECT_EQ(result[2].specifier, '}');
}

TEST_F(parse_format_string_test, placeholder_with_surrounding_text)
{
    const auto result = parse("Value: {:d}, name: {:s}");

    ASSERT_EQ(result.size(), 2);

    EXPECT_EQ(result[0].start, 7);
    EXPECT_EQ(result[0].end, 11);
    EXPECT_EQ(result[0].specifier, 'd');

    EXPECT_EQ(result[1].start, 19);
    EXPECT_EQ(result[1].end, 23);
    EXPECT_EQ(result[1].specifier, 's');
}

// Invalid format strings.

TEST_F(parse_format_string_test, rejects_nested_placeholder)
{
    EXPECT_THROW(parse("{{{"), format_error);
    EXPECT_THROW(parse("{:d{}}"), format_error);
}

TEST_F(parse_format_string_test, rejects_unmatched_opening_brace)
{
    EXPECT_THROW(parse("{"), format_error);
    EXPECT_THROW(parse("Hello {"), format_error);
}

TEST_F(parse_format_string_test, rejects_unmatched_closing_brace)
{
    EXPECT_THROW(parse("}"), format_error);
    EXPECT_THROW(parse("Hello }"), format_error);
}

TEST_F(parse_format_string_test, rejects_unsupported_specifier)
{
    EXPECT_THROW(parse("{:x}"), format_error);
    EXPECT_THROW(parse("{:i}"), format_error);
}

TEST_F(parse_format_string_test, rejects_multiple_specifier_characters)
{
    EXPECT_THROW(parse("{:dd}"), format_error);
    EXPECT_THROW(parse("{:df}"), format_error);
}

TEST_F(parse_format_string_test, rejects_empty_specifier_with_extra_delimiter)
{
    EXPECT_THROW(parse("{::}"), format_error);
}

TEST_F(parse_format_string_test, rejects_repeated_specifier_delimiter)
{
    EXPECT_THROW(parse("{::}"), format_error);
    EXPECT_THROW(parse("{::d}"), format_error);
}

TEST_F(parse_format_string_test, rejects_empty_explicit_specifier)
{
    EXPECT_THROW(parse("{:}"), format_error);
}

TEST_F(parse_format_string_test, reports_nested_placeholder_position)
{
    expect_error("{:d{}}", {1, 4});
}

TEST_F(parse_format_string_test, reports_unmatched_opening_brace_position)
{
    expect_error("Hello {", {1, 7});
}

TEST_F(parse_format_string_test, reports_unmatched_closing_brace_position)
{
    expect_error("Hello }", {1, 7});
}

TEST_F(parse_format_string_test, reports_unsupported_specifier_position)
{
    expect_error("{:x}", {1, 4});
}

TEST_F(parse_format_string_test, reports_empty_specifier_position)
{
    expect_error("{:}", {1, 3});
}

TEST_F(parse_format_string_test, reports_repeated_delimiter_position)
{
    expect_error("{::d}", {1, 3});
}

TEST_F(parse_format_string_test, reports_multiline_error_position)
{
    expect_error(
      "Hello\nworld\n}",
      {3, 1});
}

}    // namespace

}    // namespace slang::format_util
