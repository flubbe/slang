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
#include "compiler/lexer.h"
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

        slang::parser parser;
        parser.parse(slang::lexer::lex(test_input));

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

        slang::parser parser;
        parser.parse(slang::lexer::lex(test_input));

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

        slang::parser parser;
        parser.parse(slang::lexer::lex(test_input));

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
    (void)global_scope;
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
    (void)global_scope;

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

TEST(resolve, syntax_context_table_management)
{
    // Standalone table verification.
    slang::syntax_context_table table;
    auto c1 = table.allocate(slang::root_syntax_context);
    auto c2 = table.allocate(c1);

    EXPECT_NE(c1, slang::root_syntax_context);
    EXPECT_NE(c2, c1);
    EXPECT_EQ(table.parent(c1), slang::root_syntax_context);
    EXPECT_EQ(table.parent(c2), c1);
    EXPECT_EQ(table.parent(999), slang::root_syntax_context);

    // sema::env integration.
    sema::env env1;
    auto e1 = env1.syntax_contexts.allocate(slang::root_syntax_context);
    auto e2 = env1.syntax_contexts.allocate(e1);
    EXPECT_EQ(env1.syntax_contexts.parent(e1), slang::root_syntax_context);
    EXPECT_EQ(env1.syntax_contexts.parent(e2), e1);

    // Separate env instances have independent state (no global leak).
    sema::env env2;
    auto e3 = env2.syntax_contexts.allocate(slang::root_syntax_context);
    EXPECT_EQ(e3, e1);    // fresh allocation starts at root_syntax_context + 1
}

TEST(macro, expansion_distinguishes_definition_and_call_site_contexts)
{
    slang::syntax_context_table contexts;
    const auto definition_context = contexts.allocate(slang::root_syntax_context);
    const auto call_site_context = contexts.allocate(slang::root_syntax_context);

    slang::module_::macro_descriptor desc;
    slang::module_::macro_branch_descriptor branch;
    branch.capability = slang::module_::macro_expansion_capability::token_stream;
    branch.body_is_expression = true;
    branch.params.emplace_back(slang::module_::macro_parameter{"$x"});
    branch.body = slang::token_stream{std::vector<slang::token>{
      slang::token{
        "$x",
        {1, 1},
        slang::token_type::macro_identifier},
      slang::token{
        "local",
        {1, 4},
        slang::token_type::identifier}}};
    desc.branches.emplace_back(std::move(branch));

    macro::macro m{
      "test!",
      std::move(desc),
      std::nullopt,
      definition_context};

    slang::token argument{
      "argument",
      {2, 1},
      slang::token_type::identifier,
      std::nullopt,
      std::nullopt,
      false,
      {},
      {},
      99};

    const auto expanded = m.expand_macro(
      {slang::token_stream{std::vector<slang::token>{argument}}},
      call_site_context,
      contexts);

    ASSERT_EQ(expanded.size(), 2);
    EXPECT_EQ(expanded[0].context, call_site_context);
    EXPECT_NE(expanded[1].context, call_site_context);
    EXPECT_EQ(contexts.parent(expanded[1].context), definition_context);

    const auto second_expansion = m.expand_macro(
      {slang::token_stream{std::vector<slang::token>{argument}}},
      call_site_context,
      contexts);
    ASSERT_EQ(second_expansion.size(), 2);
    EXPECT_NE(second_expansion[1].context, expanded[1].context);
}

TEST(macro, expansion_uses_branch_specific_parameters)
{
    slang::module_::macro_descriptor desc;

    slang::module_::macro_branch_descriptor zero_branch;
    zero_branch.capability = slang::module_::macro_expansion_capability::token_stream;
    zero_branch.body_is_expression = true;
    zero_branch.body = slang::token_stream{std::vector<slang::token>{
      slang::token{
        "zero",
        {1, 1},
        slang::token_type::identifier}}};

    slang::module_::macro_branch_descriptor one_branch;
    one_branch.capability = slang::module_::macro_expansion_capability::token_stream;
    one_branch.body_is_expression = true;
    one_branch.params.emplace_back(slang::module_::macro_parameter{"$x"});
    one_branch.body = slang::token_stream{std::vector<slang::token>{
      slang::token{
        "$x",
        {2, 1},
        slang::token_type::macro_identifier}}};

    desc.branches.emplace_back(std::move(zero_branch));
    desc.branches.emplace_back(std::move(one_branch));

    macro::macro m{"test!", std::move(desc)};
    slang::syntax_context_table contexts;
    const auto call_site_context = contexts.allocate(slang::root_syntax_context);

    const auto zero = m.expand_macro({}, call_site_context, contexts);
    ASSERT_EQ(zero.size(), 1);
    EXPECT_EQ(zero[0].s, "zero");

    slang::token argument{
      "argument",
      {3, 1},
      slang::token_type::identifier};
    const auto one = m.expand_macro(
      {slang::token_stream{std::vector<slang::token>{argument}}},
      call_site_context,
      contexts);
    ASSERT_EQ(one.size(), 1);
    EXPECT_EQ(one[0].s, "argument");
    EXPECT_EQ(one[0].context, call_site_context);
}

TEST(macro, local_expression_body_uses_token_expansion)
{
    const std::string input =
      "macro identity! {\n"
      "  ($x: expr) => {\n"
      "    $x;\n"
      "  };\n"
      "}\n"
      "fn test() -> i32 {\n"
      "  return identity!(1 + 2);\n"
      "}\n";

    slang::parser parser;
    parser.parse(slang::lexer::lex(input));
    auto ast_root = parser.get_ast();

    sema::env sema_env;
    const_::env const_env;
    macro::env macro_env;
    co::context co_ctx{sema_env};
    ty::context type_ctx;
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx{sema_env, const_env, lowering_ctx};

    ASSERT_NO_THROW(ast_root->collect_names(co_ctx));
    ASSERT_NO_THROW(ast_root->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast_root->collect_macros(sema_env, macro_env));

    auto* descriptor_macro = macro_env.get_macro("identity!", std::nullopt);
    ASSERT_EQ(descriptor_macro->get_desc().branches.size(), 1);
    const auto& descriptor = descriptor_macro->get_desc().branches.front();
    EXPECT_EQ(
      descriptor.capability,
      slang::module_::macro_expansion_capability::token_stream);
    ASSERT_TRUE(descriptor.body.has_value());
    EXPECT_TRUE(descriptor.body_is_expression);
    ASSERT_FALSE(descriptor.body->empty());
    EXPECT_EQ(descriptor.body->back().s, ";");

    ASSERT_TRUE(ast_root->expand_macros(
      co_ctx,
      resolver_ctx,
      codegen_ctx,
      macro_env,
      sema_env.syntax_contexts,
      parser.get_macro_asts()));

    const ast::macro_invocation* invocation = nullptr;
    ast_root->visit_nodes(
      [&invocation](ast::expression& expression)
      {
          if(expression.is_macro_invocation())
          {
              invocation = expression.as_macro_invocation();
          }
      },
      true);

    ASSERT_NE(invocation, nullptr);
    ASSERT_TRUE(invocation->has_expansion());
    ASSERT_EQ(invocation->get_children().size(), 1);
    EXPECT_EQ(
      invocation->get_children().front()->get_id(),
      ast::node_identifier::binary_expression);
}

TEST(macro, local_unsupported_body_falls_back_to_ast_expansion)
{
    const std::string input =
      "macro multi! {\n"
      "  () => {\n"
      "    1;\n"
      "    2;\n"
      "  };\n"
      "}\n"
      "fn test() -> i32 {\n"
      "  multi!();\n"
      "  return 0;\n"
      "}\n";

    slang::parser parser;
    parser.parse(slang::lexer::lex(input));
    auto ast_root = parser.get_ast();

    sema::env sema_env;
    const_::env const_env;
    macro::env macro_env;
    co::context co_ctx{sema_env};
    ty::context type_ctx;
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx{sema_env, const_env, lowering_ctx};

    ASSERT_NO_THROW(ast_root->collect_names(co_ctx));
    ASSERT_NO_THROW(ast_root->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast_root->collect_macros(sema_env, macro_env));

    auto* descriptor_macro = macro_env.get_macro("multi!", std::nullopt);
    ASSERT_EQ(descriptor_macro->get_desc().branches.size(), 1);
    const auto& descriptor = descriptor_macro->get_desc().branches.front();
    EXPECT_EQ(
      descriptor.capability,
      slang::module_::macro_expansion_capability::legacy_ast);
    EXPECT_FALSE(descriptor.body.has_value());

    ASSERT_TRUE(ast_root->expand_macros(
      co_ctx,
      resolver_ctx,
      codegen_ctx,
      macro_env,
      sema_env.syntax_contexts,
      parser.get_macro_asts()));

    const ast::macro_invocation* invocation = nullptr;
    ast_root->visit_nodes(
      [&invocation](ast::expression& expression)
      {
          if(expression.is_macro_invocation())
          {
              invocation = expression.as_macro_invocation();
          }
      },
      true);

    ASSERT_NE(invocation, nullptr);
    ASSERT_TRUE(invocation->has_expansion());
    ASSERT_EQ(invocation->get_children().size(), 1);
    EXPECT_EQ(
      invocation->get_children().front()->get_id(),
      ast::node_identifier::macro_branch);
}

}    // namespace
