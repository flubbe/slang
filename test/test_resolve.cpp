/**
 * slang - a simple scripting language.
 *
 * name resolution tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "compiler/codegen/codegen.h"
#include "compiler/loader.h"
#include "compiler/macro.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace const_ = slang::const_;
namespace ld = slang::loader;
namespace macro = slang::macro;
namespace rs = slang::resolve;
namespace sema = slang::sema;
namespace tl = slang::lowering;
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
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        co::context co_ctx{sema_env};
        ty::context type_ctx;
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx{sema_env, const_env, lowering_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
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
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        co::context co_ctx{sema_env};
        ty::context type_ctx;
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx{sema_env, const_env, lowering_ctx};

        // necessary since we're not using the module loader.
        ASSERT_NO_THROW(loader_ctx.resolve_module("std", false));

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
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
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        co::context co_ctx{sema_env};
        ty::context type_ctx;
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx{sema_env, const_env, lowering_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(resolver_ctx.resolve_imports(loader_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        EXPECT_THROW(ast->type_check(type_ctx, sema_env), ty::type_error);
    }
}

TEST(resolve, syntax_context_hygiene)
{
    sema::env sema_env;
    co::context co_ctx{sema_env};
    const_::env const_env;
    macro::env macro_env;
    ty::context type_ctx;
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};

    // First scope pushed is the global scope.
    auto global_scope = co_ctx.push_scope(std::string{"global"}, {1, 1});
    // Create a local scope.
    auto local_scope = co_ctx.push_scope(std::string{"local_scope"}, {2, 1});

    // Declare variable 'x' with root syntax context (0).
    const slang::syntax_context_id ctx_a = slang::root_syntax_context;
    const slang::syntax_context_id ctx_b = 1;

    slang::token tok_a{"x", {2, 5}, slang::token_type::identifier, std::nullopt, std::nullopt, false, {}, {}, ctx_a};
    slang::token tok_b{"x", {3, 5}, slang::token_type::identifier, std::nullopt, std::nullopt, false, {}, {}, ctx_b};

    auto id_a = co_ctx.declare(tok_a, "x", sema::symbol_type::variable, sema::symbol_id::invalid, false);
    EXPECT_NE(id_a, sema::symbol_id::invalid);

    // Declaring 'x' with a different syntax context should succeed (no redefinition collision).
    sema::symbol_id id_b = sema::symbol_id::invalid;
    EXPECT_NO_THROW({
        id_b = co_ctx.declare(tok_b, "x", sema::symbol_type::variable, sema::symbol_id::invalid, false);
    });
    EXPECT_NE(id_b, sema::symbol_id::invalid);
    EXPECT_NE(id_a, id_b);

    // Resolving with ctx_a should find id_a.
    auto res_a = resolver_ctx.resolve("x", sema::symbol_type::variable, local_scope, ctx_a);
    ASSERT_TRUE(res_a.has_value());
    EXPECT_EQ(res_a.value(), id_a);

    // Resolving with ctx_b should find id_b.
    auto res_b = resolver_ctx.resolve("x", sema::symbol_type::variable, local_scope, ctx_b);
    ASSERT_TRUE(res_b.has_value());
    EXPECT_EQ(res_b.value(), id_b);

    // Resolving with an unknown context ctx_c (e.g. 2) should fail (no variable capture).
    const slang::syntax_context_id ctx_c = 2;
    auto res_c = resolver_ctx.resolve("x", sema::symbol_type::variable, local_scope, ctx_c);
    EXPECT_FALSE(res_c.has_value());

    // Check that re-declaring with the same name AND same context still throws redefinition_error.
    EXPECT_THROW(
      co_ctx.declare(tok_a, "x", sema::symbol_type::variable, sema::symbol_id::invalid, false),
      co::redefinition_error);

    co_ctx.pop_scope();
    co_ctx.pop_scope();
}

TEST(resolve, syntax_context_global_fallback)
{
    sema::env sema_env;
    co::context co_ctx{sema_env};
    const_::env const_env;
    macro::env macro_env;
    ty::context type_ctx;
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};

    // First scope pushed is the global scope.
    auto global_scope = co_ctx.push_scope(std::string{"global"}, {1, 1});

    // Declare a function 'foo' in global scope with root context.
    slang::token fn_tok{"foo", {1, 1}, slang::token_type::identifier, std::nullopt, std::nullopt, false, {}, {}, slang::root_syntax_context};
    auto fn_id = co_ctx.declare(fn_tok, "foo", sema::symbol_type::function, sema::symbol_id::invalid, false);
    EXPECT_NE(fn_id, sema::symbol_id::invalid);

    // Enter an inner scope.
    auto inner_scope = co_ctx.push_scope(std::string{"inner_scope"}, {2, 1});

    // A lookup with non-root syntax context inside the inner scope should fall back to global scope root context.
    const slang::syntax_context_id macro_ctx = 42;
    auto res = resolver_ctx.resolve("foo", sema::symbol_type::function, inner_scope, macro_ctx);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res.value(), fn_id);

    co_ctx.pop_scope();
    co_ctx.pop_scope();
}

}    // namespace
