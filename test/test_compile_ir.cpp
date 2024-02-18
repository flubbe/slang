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

}    // namespace