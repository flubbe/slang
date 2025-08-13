/**
 * slang - a simple scripting language.
 *
 * name resolution tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "compiler/codegen.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "loader.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace ld = slang::loader;
namespace rs = slang::resolve;
namespace sema = slang::sema;
namespace ty = slang::typing;

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

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;
        mgr.add_search_path("lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ld::context loader_ctx{mgr};
        sema::env env;
        co::context co_ctx{env};
        rs::context resolver_ctx{env};
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, env));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    }
    {
        const std::string test_input =
          "import std;\n"
          "fn main() -> i32 {\n"
          " std::println(\"Hello, World!\");\n"
          " return 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;
        mgr.add_search_path("lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ld::context loader_ctx{mgr};
        sema::env env;
        co::context co_ctx{env};
        rs::context resolver_ctx{env};
        ty::context type_ctx;
        cg::context codegen_ctx;

        // necessary since we're not using the module loader.
        ASSERT_NO_THROW(loader_ctx.resolve_module("std", false));

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, env));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    }
    {
        const std::string test_input =
          "import std;\n"
          "fn main() -> i32 {\n"
          " std::println(1);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;
        mgr.add_search_path("lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ld::context loader_ctx{mgr};
        sema::env env;
        co::context co_ctx{env};
        rs::context resolver_ctx{env};
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, env));
        EXPECT_THROW(ast->type_check(type_ctx, env), ty::type_error);
    }
}

}    // namespace
