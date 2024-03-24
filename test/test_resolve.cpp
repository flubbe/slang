/**
 * slang - a simple scripting language.
 *
 * name resolution tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "parser.h"
#include "codegen.h"
#include "resolve.h"
#include "typing.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;

namespace
{

TEST(resolve, std)
{
    {
        const std::string test_input =
          "import std;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        ty::context type_ctx;
        rs::context resolve_ctx;
        EXPECT_NO_THROW(ast->collect_names(type_ctx));
        EXPECT_NO_THROW(resolve_ctx.resolve_imports(type_ctx));
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

}