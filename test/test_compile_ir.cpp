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
}

}    // namespace