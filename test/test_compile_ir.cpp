/**
 * slang - a simple scripting language.
 *
 * Compilation to IR tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "parser.h"
#include "codegen.h"

namespace cg = slang::codegen;

namespace
{

TEST(compile_ir, empty)
{
    const std::string test_input = "";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    const slang::ast::block* ast = parser.get_ast();
    EXPECT_NE(ast, nullptr);

    cg::context ctx;
    ast->generate_code(&ctx);

    EXPECT_EQ(ctx.to_string().length(), 0);
}

TEST(compile_ir, empty_function)
{
    const std::string test_input =
      "fn f() -> void\n"
      "{\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    const slang::ast::block* ast = parser.get_ast();
    EXPECT_NE(ast, nullptr);

    cg::context ctx;
    ast->generate_code(&ctx);

    EXPECT_EQ(ctx.to_string(),
              "define void @f() {\n"
              "entry:\n"
              " ret\n"
              "}");
}

TEST(compile_ir, builtin_return_values)
{
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " return 1;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> f32\n"
          "{\n"
          " return 1.323;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define f32 @f() {\n"
                  "entry:\n"
                  " const f32 1.323\n"
                  " ret f32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> str\n"
          "{\n"
          " return \"test\";\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  ".string @0 \"test\"\n"
                  "define str @f() {\n"
                  "entry:\n"
                  " load str @0\n"
                  " ret str\n"
                  "}");
    }
}

TEST(compile_ir, function_arguments_and_locals)
{
    {
        const std::string test_input =
          "fn f(i: i32, j: str, k: f32) -> void\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %i, str %j, f32 %k) {\n"
                  "entry:\n"
                  " ret\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f(i: i32, j: str, k: f32) -> i32\n"
          "{\n"
          " let a: i32 = 1;\n"
          " return a;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, str %j, f32 %k) {\n"
                  "local i32 %a\n"
                  "entry:\n"
                  " const i32 1\n"
                  " store i32 %a\n"
                  " load i32 %a\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " let a: i32 = j;\n"
          " return a;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "local i32 %a\n"
                  "entry:\n"
                  " load i32 %j\n"
                  " store i32 %a\n"
                  " load i32 %a\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " i = 3;\n"
          " return j;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " store i32 %i\n"
                  " load i32 %j\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " i = j = 3;\n"
          " return j;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " store i32 %j\n"
                  " load i32 %j\n"
                  " store i32 %i\n"
                  " load i32 %j\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, binary_operators)
{
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 1*2 + 3;\n"
          " return i;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        cg::context ctx;
        ast->generate_code(&ctx);

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " mul i32\n"
                  " const i32 3\n"
                  " add i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
                  " ret i32\n"
                  "}");
    }
}

}    // namespace