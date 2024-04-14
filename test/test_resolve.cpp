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

        slang::file_manager mgr;
        mgr.add_search_path("src/lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    }
    {
        const std::string test_input =
          "import std;\n"
          "fn main() -> i32 {\n"
          " println(\"Hello, World!\");\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("src/lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    }
    {
        const std::string test_input =
          "import std;\n"
          "fn main() -> i32 {\n"
          " println(1);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("src/lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

}    // namespace