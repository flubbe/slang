/**
 * slang - a simple scripting language.
 *
 * Lexer tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <variant>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "compiler/lexer.h"

namespace
{

TEST(lexer, tokenize_whitespaces_identifiers)
{
    const std::string test_string =
      "a a_b _AB\t_12ab\nh\ru_789\vt";

    slang::lexer lexer{test_string};
    lexer.set_tab_size(4);

    std::vector<slang::token> tokens;
    std::optional<slang::token> t;

    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 7);
    if(tokens.size() != 7)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "a");
    EXPECT_EQ(tokens[0].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[0].location, slang::token_location(1, 1));

    EXPECT_EQ(tokens[1].s, "a_b");
    EXPECT_EQ(tokens[1].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[1].location, slang::token_location(1, 3));

    EXPECT_EQ(tokens[2].s, "_AB");
    EXPECT_EQ(tokens[2].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[2].location, slang::token_location(1, 7));

    EXPECT_EQ(tokens[3].s, "_12ab");
    EXPECT_EQ(tokens[3].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[3].location, slang::token_location(1, 14));

    EXPECT_EQ(tokens[4].s, "h");
    EXPECT_EQ(tokens[4].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[4].location, slang::token_location(2, 1));

    EXPECT_EQ(tokens[5].s, "u_789");
    EXPECT_EQ(tokens[5].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[5].location, slang::token_location(2, 1));

    EXPECT_EQ(tokens[6].s, "t");
    EXPECT_EQ(tokens[6].type, slang::token_type::identifier);
    EXPECT_EQ(tokens[6].location, slang::token_location(3, 6));
}

TEST(lexer, single_line_comment)
{
    const std::string test_string =
      "// This is a single-line comment\n"
      "a b cde // More comment";

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 3);
    if(tokens.size() != 3)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "a");
    EXPECT_EQ(tokens[0].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[1].s, "b");
    EXPECT_EQ(tokens[1].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[2].s, "cde");
    EXPECT_EQ(tokens[2].type, slang::token_type::identifier);
}

TEST(lexer, multi_line_comment)
{
    const std::string test_string =
      "/* This is a multi-line comment */\n"
      "cde /* More comment\n"
      "that continues here */ f gh";

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 3);
    if(tokens.size() != 3)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "cde");
    EXPECT_EQ(tokens[0].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[1].s, "f");
    EXPECT_EQ(tokens[1].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[2].s, "gh");
    EXPECT_EQ(tokens[2].type, slang::token_type::identifier);
}

TEST(lexer, operators)
{
    const std::string test_string =
      "+++++ --- <<< <> <<=+ > !%& + - %::=";

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 20);
    if(tokens.size() != 20)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "++");
    EXPECT_EQ(tokens[1].s, "++");
    EXPECT_EQ(tokens[2].s, "+");

    EXPECT_EQ(tokens[3].s, "--");
    EXPECT_EQ(tokens[4].s, "-");

    EXPECT_EQ(tokens[5].s, "<<");
    EXPECT_EQ(tokens[6].s, "<");

    EXPECT_EQ(tokens[7].s, "<");
    EXPECT_EQ(tokens[8].s, ">");

    EXPECT_EQ(tokens[9].s, "<<=");
    EXPECT_EQ(tokens[10].s, "+");

    EXPECT_EQ(tokens[11].s, ">");

    EXPECT_EQ(tokens[12].s, "!");
    EXPECT_EQ(tokens[13].s, "%");
    EXPECT_EQ(tokens[14].s, "&");

    EXPECT_EQ(tokens[15].s, "+");
    EXPECT_EQ(tokens[16].s, "-");

    EXPECT_EQ(tokens[17].s, "%");
    EXPECT_EQ(tokens[18].s, "::");
    EXPECT_EQ(tokens[19].s, "=");
}

TEST(lexer, int_literals)
{
    const std::string test_string =
      "1 2 123 0x12 0xab34";    // NOTE integer literals are always unsigned.

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 5);
    if(tokens.size() != 5)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "1");
    EXPECT_EQ(tokens[0].type, slang::token_type::int_literal);
    EXPECT_TRUE(tokens[0].value);
    if(tokens[0].value)
    {
        EXPECT_EQ(std::get<int>(*tokens[0].value), 1);
    }

    EXPECT_EQ(tokens[1].s, "2");
    EXPECT_EQ(tokens[1].type, slang::token_type::int_literal);
    EXPECT_TRUE(tokens[1].value);
    if(tokens[1].value)
    {
        EXPECT_EQ(std::get<int>(*tokens[1].value), 2);
    }

    EXPECT_EQ(tokens[2].s, "123");
    EXPECT_EQ(tokens[2].type, slang::token_type::int_literal);
    EXPECT_TRUE(tokens[2].value);
    if(tokens[2].value)
    {
        EXPECT_EQ(std::get<int>(*tokens[2].value), 123);
    }

    EXPECT_EQ(tokens[3].s, "0x12");
    EXPECT_EQ(tokens[3].type, slang::token_type::int_literal);
    EXPECT_TRUE(tokens[3].value);
    if(tokens[3].value)
    {
        EXPECT_EQ(std::get<int>(*tokens[3].value), 0x12);
    }

    EXPECT_EQ(tokens[4].s, "0xab34");
    EXPECT_EQ(tokens[4].type, slang::token_type::int_literal);
    EXPECT_TRUE(tokens[4].value);
    if(tokens[4].value)
    {
        EXPECT_EQ(std::get<int>(*tokens[4].value), 0xab34);
    }
}

TEST(lexer, fp_literals)
{
    const std::string test_string =
      "1. 2.23 12.3 12e7 12e-3 1.3E5 1.2e-8";    // NOTE integer literals are always unsigned.

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 7);
    if(tokens.size() != 7)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "1.");
    EXPECT_EQ(tokens[0].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[0].value);
    if(tokens[0].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[0].value), 1., 1e-6);
    }

    EXPECT_EQ(tokens[1].s, "2.23");
    EXPECT_EQ(tokens[1].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[1].value);
    if(tokens[1].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[1].value), 2.23, 1e-6);
    }

    EXPECT_EQ(tokens[2].s, "12.3");
    EXPECT_EQ(tokens[2].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[2].value);
    if(tokens[2].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[2].value), 12.3, 1e-6);
    }

    EXPECT_EQ(tokens[3].s, "12e7");
    EXPECT_EQ(tokens[3].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[3].value);
    if(tokens[3].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[3].value), 12e7, 1e-6);
    }

    EXPECT_EQ(tokens[4].s, "12e-3");
    EXPECT_EQ(tokens[4].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[4].value);
    if(tokens[4].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[4].value), 12e-3, 1e-6);
    }

    EXPECT_EQ(tokens[5].s, "1.3E5");
    EXPECT_EQ(tokens[5].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[5].value);
    if(tokens[5].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[5].value), 1.3E5, 1e-6);
    }

    EXPECT_EQ(tokens[6].s, "1.2e-8");
    EXPECT_EQ(tokens[6].type, slang::token_type::fp_literal);
    EXPECT_TRUE(tokens[6].value);
    if(tokens[6].value)
    {
        EXPECT_NEAR(std::get<float>(*tokens[6].value), 1.2e-8, 1e-6);
    }
}

TEST(lexer, string_literals)
{
    const std::string test_string =
      "a b \"s t r i n g\" \"1.23\" \"123\" c";

    slang::lexer lexer{test_string};
    std::vector<slang::token> tokens;

    std::optional<slang::token> t;
    while((t = lexer.next()) != std::nullopt)
    {
        tokens.push_back(*t);
    }
    EXPECT_TRUE(lexer.eof());

    EXPECT_EQ(tokens.size(), 6);
    if(tokens.size() != 6)
    {
        GTEST_SKIP();
    }

    EXPECT_EQ(tokens[0].s, "a");
    EXPECT_EQ(tokens[0].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[1].s, "b");
    EXPECT_EQ(tokens[1].type, slang::token_type::identifier);

    EXPECT_EQ(tokens[2].s, "\"s t r i n g\"");
    EXPECT_EQ(tokens[2].type, slang::token_type::str_literal);
    EXPECT_TRUE(tokens[2].value);
    if(tokens[2].value)
    {
        EXPECT_EQ(std::get<std::string>(*tokens[2].value), "s t r i n g");
    }

    EXPECT_EQ(tokens[3].s, "\"1.23\"");
    EXPECT_EQ(tokens[3].type, slang::token_type::str_literal);
    EXPECT_TRUE(tokens[3].value);
    if(tokens[3].value)
    {
        EXPECT_EQ(std::get<std::string>(*tokens[3].value), "1.23");
    }

    EXPECT_EQ(tokens[4].s, "\"123\"");
    EXPECT_EQ(tokens[4].type, slang::token_type::str_literal);
    EXPECT_TRUE(tokens[4].value);
    if(tokens[4].value)
    {
        EXPECT_EQ(std::get<std::string>(*tokens[4].value), "123");
    }

    EXPECT_EQ(tokens[5].s, "c");
    EXPECT_EQ(tokens[5].type, slang::token_type::identifier);
}

TEST(lexer, fail_literals)
{
    slang::lexer lexer;
    std::vector<slang::token> tokens;
    std::optional<slang::token> t;

    // no tokens parsed.
    EXPECT_EQ(lexer.next(), std::nullopt);
    EXPECT_TRUE(lexer.eof());

    // invalid suffix
    lexer.set_input("1a");
    EXPECT_THROW(lexer.next(), slang::lexical_error);

    // invalid suffix
    lexer.set_input("1.2b");
    EXPECT_THROW(lexer.next(), slang::lexical_error);

    // missing terminating quote
    lexer.set_input("\"missing quote\n");
    EXPECT_THROW(lexer.next(), slang::lexical_error);

    // invalid suffix
    lexer.set_input("\"string\"s");
    EXPECT_THROW(lexer.next(), slang::lexical_error);
}

TEST(lexer, example_program)
{
    auto run = []()
    {
        const std::string test_string =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\tstd::print(\"Hello, World!\\n\");\n"
          "}";

        slang::lexer lexer{test_string};

        std::optional<slang::token> t;
        while((t = lexer.next()) != std::nullopt)
            ;

        EXPECT_TRUE(lexer.eof());
    };

    EXPECT_NO_THROW(run());
}

}    // namespace
