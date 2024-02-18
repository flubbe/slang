/**
 * slang - a simple scripting language.
 *
 * Parser tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <variant>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "parser.h"

namespace
{

TEST(parser, import_statement)
{
    {
        const std::string test_input =
          "import std;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import a::b::c;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import a";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "import a.b;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "import a + b;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, function)
{
    {
        const std::string test_input =
          "fn f(s: string) -> i32 {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f(s: string, i: i32, f: f32) -> i32 {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f(s: string) -> i32 {a = b; c;}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        // test incomplete definition.
        const std::string test_input =
          "fn f(s: ";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test incomplete definition.
        const std::string test_input =
          "fn f(s: string)";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test missing return type.
        const std::string test_input =
          "fn f(s: string) {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test missing/incomplete return type.
        const std::string test_input =
          "fn f(s: string) -> {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test incomplete function body.
        const std::string test_input =
          "fn f(s: string) -> i32 {";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test nested function definitions.
        const std::string test_input =
          "fn f(s: string) -> i32 { fn g() -> void {} }";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, return_statement)
{
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{\n"
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{\n"
          "\treturn +1234.5;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{\n"
          "\treturn -1+2*3;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{\n"
          "\treturn \"Test\";\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{\n"
          "\treturn 0\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: string) -> i32\n"
          "{"
          "return 0";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, operators)
{
    const std::string test_input =
      "let a: i32 = 1 + b * (c = 3 = 4) / 5 & 6;";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, hello_world)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      "\tstd::print(\"Hello, World!\\n\");\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, function_call)
{
    {
        const std::string test_input =
          "fn main(s: string) -> i32\n"
          "{\n"
          " return add(+4, -5) + mul(-7, add(1,-2));\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn main(s: string) -> i32\n"
          "{\n"
          " return add(sub(2, 3+4.3), 5) + mul(7, add(1,2));\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn main(s: string) -> i32\n"
          "{\n"
          " return add(+4, *5) + mul(-7, add(1,-2));\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, if_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " if(1 == 2)\n"
      " {\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, if_else_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " if(1 == 2)\n"
      " {\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      " else\n"
      " {\n"
      "  std::print(\"!dlroW ,olleH\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, if_elseif_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " if(1 == 2 && 3 != 3)\n"
      " {\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      " else if(1 + 2 == 3)\n"
      " {\n"
      "  std::print(\"!dlroW ,olleH\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " while(1 == 2 || 3)\n"
      " {\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_if_break_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " while(1 == 2 || 3)\n"
      " {\n"
      "  if(0)\n"
      "  {\n"
      "   break;\n"
      "  }\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_if_continue_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " while(1 == 2 || 3)\n"
      " {\n"
      "  if(1 - 1)\n"
      "  {\n"
      "   continue;\n"
      "  }\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, variable_declaration)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "let k : i32 = 3;\n"
      "\n"
      "fn main(s: string) -> i32\n"
      "{\n"
      " let a : f32;\n"
      " let b : f32 = 2*a;"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, struct_definition)
{
    const std::string test_input =
      "struct S\n"
      "{\n"
      " a: i32,\n"
      " b: f32\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, struct_initialization)
{
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32\n"
          "};\n"
          "let s1: S = S{123};\n"
          "let s2: S = S{a: 124, b: 2};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32\n"
          "};\n"
          "let s1: S = S{123, a: 2};\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32\n"
          "};\n"
          "let s1: S = S{a: 123, 2};\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, struct_member_access)
{
    {
        const std::string test_input =
          "let s1: i32 = s.a;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "let s1: i32 = s.a.b + t.c;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "let s1: i32 = s..c;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

}    // namespace