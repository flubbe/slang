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
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import a::b::c;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

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
          "fn f(s: str) -> i32 {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f(s: str, i: i32, f: f32) -> i32 {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f(s: str) -> i32 {a = b; c;}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f() -> void {}\n"
          "fn g() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i32;\n"
          " a = -23;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

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
          "fn f(s: str)";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test missing return type.
        const std::string test_input =
          "fn f(s: str) {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test missing/incomplete return type.
        const std::string test_input =
          "fn f(s: str) -> {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test incomplete function body.
        const std::string test_input =
          "fn f(s: str) -> i32 {";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test nested function definitions.
        const std::string test_input =
          "fn f(s: str) -> i32 { fn g() -> void {} }";

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
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\treturn +1234.5;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\treturn -1+2*3;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\treturn \"Test\";\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
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
          "fn main(args: [str]) -> i32\n"
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
    {
        const std::string test_input =
          "let a: i32 = 1 + 1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected =
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=a, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                  "op=\"+\", "
                  "lhs=IntLiteral("
                    "value=1"
                  "), "
                  "rhs=IntLiteral("
                    "value=1"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
    {
        const std::string test_input =
          "let a: i32 = 2 * 1 + 1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected = 
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=a, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                "op=\"+\", "
                "lhs=Binary("
                  "op=\"*\", "
                  "lhs=IntLiteral("
                    "value=2"
                  "), "
                  "rhs=IntLiteral("
                    "value=1"
                  ")"
                "), "
                "rhs=IntLiteral("
                  "value=1"
                ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on
        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
    {
        const std::string test_input =
          "let a: i32 = 2 + 1 * 1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected = 
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=a, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                  "op=\"+\", "
                  "lhs=IntLiteral("
                    "value=2"
                  "), "
                  "rhs=Binary("
                    "op=\"*\", "
                    "lhs=IntLiteral("
                      "value=1"
                    "), "
                    "rhs=IntLiteral("
                      "value=1"
                    ")"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on
        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
    {
        const std::string test_input =
          "let a: i32 = b = c = 1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected = 
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=a, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                  "op=\"=\", "
                  "lhs=VariableReference("
                    "name=b"
                  "), "
                  "rhs=Binary("
                    "op=\"=\", "
                    "lhs=VariableReference("
                      "name=c"
                    "), "
                    "rhs=IntLiteral("
                      "value=1"
                    ")"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on
        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
    {
        const std::string test_input =
          "let a: i32 = 1 + b * (c = 3 = 4) / 5 & 6;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "let s: i32 = ++a * --b;\n"
          "let t: i32 = a++ * b--;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected =
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=s, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                  "op=\"*\", "
                  "lhs=Unary(op=\"++\", "
                  "operand=VariableReference("
                    "name=a"
                  ")"
                "), "
                "rhs=Unary("
                  "op=\"--\", "
                    "operand=VariableReference("
                      "name=b"
                    ")"
                  ")"
                ")"
              "), "
              "VariableDeclaration("
                "name=t, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Binary("
                  "op=\"*\", "
                  "lhs=Postfix("
                    "identifier=VariableReference("
                      "name=a"
                    "), "
                    "op=\"++\""
                  "), "
                  "rhs=Postfix("
                    "identifier=VariableReference("
                      "name=b"
                    "), "
                    "op=\"--\""
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
}

TEST(parser, hello_world)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
      "{\n"
      "\tstd::print(\"Hello, World!\\n\");\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, function_call)
{
    {
        const std::string test_input =
          "fn main(args: [str]) -> i32\n"
          "{\n"
          " return add(+4, -5) + mul(-7, add(1,-2));\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn main(args: [str]) -> i32\n"
          "{\n"
          " return add(sub(2, 3+4.3), 5) + mul(7, add(1,2));\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn main(args: [str]) -> i32\n"
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
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          " if(1 == 2)\n"
          " {\n"
          "  std::print(\"Hello, World!\\n\");\n"
          " }\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          " if(1 == 2)\n"
          " {\n"
          "  std::print(\"Hello, World!\\n\");\n"
          " }\n"
          " let a: i32 = 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
}

TEST(parser, if_else_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
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
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, if_elseif_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
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
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
      "{\n"
      " while(1 == 2 || 3)\n"
      " {\n"
      "  std::print(\"Hello, World!\\n\");\n"
      " }\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_if_break_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
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
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, while_if_continue_statement)
{
    const std::string test_input =
      "import std;\n"
      "\n"
      "fn main(args: [str]) -> i32\n"
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
    EXPECT_NO_THROW(parser.parse(lexer));

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, variable_declaration)
{
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "let k : i32 = 3;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          " let a : f32;\n"
          " let b : f32 = 2*a;"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        // clang-format off
        const std::string expected =
          "Block("
            "exprs=("
              "Function("
                "prototype=Prototype("
                  "name=f, "
                  "return_type=TypeExpression("
                    "name=void, "
                    "namespaces=(), "
                    "array=false"
                  "), "
                  "args=()"
                "), "
                "body=Block("
                  "exprs=("
                    "VariableDeclaration("
                      "name=b, "
                      "type=TypeExpression("
                        "name=i32, "
                        "namespaces=(), "
                        "array=true"
                      "), "
                      "expr=ArrayInitializer("
                        "exprs=("
                          "IntLiteral("
                            "value=1"
                          "), "
                          "IntLiteral("
                            "value=2"
                          ")"
                        ")"
                      ")"
                    ")"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "let 1: i32 = 2;";    // non-identifier as name.

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "let if: i32 = 2;";    // keyword as name.

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "let i: i32 = 2";    // missing semicolon.

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, explicit_cast)
{
    const std::string test_input =
      "let k: i32 = 3.0 as i32;";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    ASSERT_NO_THROW(parser.parse(lexer));

    // clang-format off
    const std::string expected =
      "Block("
        "exprs=("
          "VariableDeclaration("
            "name=k, "
            "type=TypeExpression("
              "name=i32, "
              "namespaces=(), "
              "array=false"
            "), "
            "expr=TypeCast("
              "target_type=TypeExpression("
               "name=i32, "
               "namespaces=(), "
               "array=false"
              "), "
              "expr=FloatLiteral("
                "value=3"
              ")"
            ")"
          ")"
        ")"
      ")";
    // clang-format on

    EXPECT_EQ(parser.get_ast()->to_string(), expected);

    EXPECT_TRUE(lexer.eof());
}

TEST(parser, struct_definition)
{
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
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
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          "};\n"
          "fn f() -> void\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "fn f(i: f32) -> void\n"
          "{\n"
          " struct S{};\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
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
        EXPECT_NO_THROW(parser.parse(lexer));

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
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected =
          "Block("
            "exprs=("
              "VariableDeclaration("
                "name=s1, "
                "type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=false"
                "), "
                "expr=Access("
                  "lhs=VariableReference("
                    "name=s"
                  "), "
                  "rhs=VariableReference("
                    "name=a"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
    {
        const std::string test_input =
          "let s1: i32 = s.a.b + t.c;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

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

TEST(parser, struct_cast)
{
    {
        const std::string test_input =
          "fn test() -> void\n"
          "{\n"
          "    (s as S).v = 12;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        // clang-format off
        const std::string expected =
          "Block("
            "exprs=("
              "Function("
                "prototype=Prototype("
                  "name=test, "
                  "return_type=TypeExpression("
                    "name=void, "
                    "namespaces=(), "
                    "array=false"
                  "), "
                  "args=()"
                "), "
                "body=Block("
                  "exprs=("
                    "Binary("
                      "op=\"=\", "
                      "lhs=Access("
                        "lhs=TypeCast("
                          "target_type=TypeExpression("
                            "name=S, "
                            "namespaces=(), "
                            "array=false"
                          "), "
                          "expr=VariableReference("
                            "name=s"
                          ")"
                        "), "
                        "rhs=VariableReference("
                          "name=v"
                        ")"
                      "), "
                      "rhs=IntLiteral("
                        "value=12"
                      ")"
                    ")"
                  ")"
                ")"
              ")"
            ")"
          ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
}

TEST(parser, null)
{
    {
        const std::string test_input =
          "let s: S = null;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));
    }
    {
        const std::string test_input =
          "fn test() -> void\n"
          "{\n"
          " let null: i32 = 1;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "struct null {\n"
          " i: i32\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, directives)
{
    {
        const std::string test_input =
          "#[native(lib=\"test\")]\n"
          "fn f() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "#[test]\n"
          "fn f() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "#[test()]\n"
          "fn f() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());
    }
    {
        const std::string test_input =
          "#[test(]\n"
          "fn f() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "#[test()\n"
          "fn f() -> void {}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(parser, array_return)
{
    {
        const std::string test_input =
          "fn f() -> [i32] { return [1, 2]; }";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));
    }
    {
        const std::string test_input =
          "fn f() -> void { let a: i32 = g()[0]; }";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));
    }
    {
        const std::string test_input =
          "fn f() -> i32 { return g()[0]; }";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_NO_THROW(parser.parse(lexer));
    }
}

TEST(parser, struct_array_access)
{
    {
        const std::string test_input =
          "struct L{f: f32};\n"
          "fn f() -> [i32] {\n"
          " let l: [L] = new L[1];\n"
          " l[0].f = 123.0;\n"
          " return l[0].f as i32;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        // clang-format off
        const std::string expected =
        "Block("
          "exprs=("
            "Struct("
              "name=L, "
              "members=("
                "VariableDeclaration("
                  "name=f, "
                  "type=TypeExpression("
                    "name=f32, "
                    "namespaces=(), "
                    "array=false"
                  "), "
                  "expr=<none>"
                ")"
              ")"
            "), "
            "Function("
              "prototype=Prototype("
                "name=f, "
                "return_type=TypeExpression("
                  "name=i32, "
                  "namespaces=(), "
                  "array=true"
                "), "
                "args=()"
              "), "
              "body=Block("
                "exprs=("
                  "VariableDeclaration(name=l, "
                    "type=TypeExpression("
                      "name=L, "
                      "namespaces=(), "
                      "array=true"
                    "), "
                    "expr=NewExpression("
                      "type=TypeExpression("
                        "name=L, "
                        "namespaces=(), "
                        "array=false"
                      "), "
                      "expr=IntLiteral("
                        "value=1"
                      ")"
                    ")"
                  "), "
                  "Binary("
                    "op=\"=\", "
                    "lhs=Access("
                      "lhs=VariableReference("
                        "name=l, "
                        "element_expr=IntLiteral("
                          "value=0"
                        ")"
                      "), "
                      "rhs=VariableReference("
                        "name=f"
                      ")"
                    "), "
                    "rhs=FloatLiteral("
                      "value=123"
                    ")"
                  "), "
                  "Return("
                    "expr=TypeCast("
                      "target_type=TypeExpression("
                        "name=i32, "
                        "namespaces=(), "
                        "array=false"
                      "), "
                      "expr=Access("
                        "lhs=VariableReference("
                          "name=l, "
                          "element_expr=IntLiteral("
                            "value=0"
                          ")"
                        "), "
                        "rhs=VariableReference("
                          "name=f"
                        ")"
                      ")"
                    ")"
                  ")"
                ")"
              ")"
            ")"
          ")"
        ")";
        // clang-format on

        EXPECT_EQ(parser.get_ast()->to_string(), expected);
    }
}

}    // namespace
