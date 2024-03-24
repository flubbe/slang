/**
 * slang - a simple scripting language.
 *
 * Compiler output tests.
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
#include "typing.h"
#include "resolve.h"
#include "module.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;

namespace
{

TEST(output, native_binding)
{
    {
        const std::string test_input =
          "/**\n"
          " * Print a string to stdout.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=slang)]\n"
          "fn print(s: str) -> void;\n"
          "\n"
          "/**\n"
          " * Print a string to stdout and append a new-line character.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=slang)]\n"
          "fn println(s: str) -> void;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        ty::context type_ctx;
        rs::context resolve_ctx;
        cg::context codegen_ctx;

        EXPECT_NO_THROW(ast->collect_names(type_ctx));
        EXPECT_NO_THROW(resolve_ctx.resolve_imports(type_ctx));
        EXPECT_NO_THROW(ast->type_check(type_ctx));
        EXPECT_NO_THROW(ast->generate_code(&codegen_ctx));
        EXPECT_NO_THROW(codegen_ctx.finalize());

        slang::language_module mod = codegen_ctx.to_module();
        // TODO validate module.
    }
}

}    // namespace