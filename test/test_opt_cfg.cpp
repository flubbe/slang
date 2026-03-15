/**
 * slang - a simple scripting language.
 *
 * Control flow graph optimization tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>

#include <gtest/gtest.h>

#include "compiler/cfg/simplify.h"
#include "compiler/codegen/codegen.h"
#include "compiler/macro.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace const_ = slang::const_;
namespace macro = slang::macro;
namespace rs = slang::resolve;
namespace sema = slang::sema;
namespace ty = slang::typing;
namespace tl = slang::lowering;

namespace
{

cg::context get_context(
  sema::env& sema_env,
  const_::env& const_env,
  tl::context& lowering_ctx)
{
    cg::context ctx{sema_env, const_env, lowering_ctx};
    ctx.clear_flag(cg::codegen_flags::enable_const_eval);
    return ctx;
}

TEST(opt_cfg, remove_unreachable_blocks)
{
    const std::string test_input = R"(
        fn f() -> i32
        {
            return 12;
            return 13;

            let i: i32 = 123;
            if(i == 123)
            {
                return -1;
            }

            return 0;
        }
        )";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    std::shared_ptr<ast::expression> ast = parser.get_ast();

    sema::env sema_env;
    const_::env const_env;
    macro::env macro_env;
    ty::context type_ctx;
    co::context co_ctx{sema_env};
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx = get_context(sema_env, const_env, lowering_ctx);
    slang::cfg::simplify cfg_context{codegen_ctx};

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->define_types(type_ctx));
    ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    ASSERT_NO_THROW(cfg_context.run());

    EXPECT_EQ(codegen_ctx.to_string(),
              R"(define i32 @f() {
local i32 %1
entry:
 const i32 12
 ret i32
})");
}

TEST(opt_cfg, while_loop_unreachable_blocks)
{
    {
        const std::string test_input =
          "const a: i32 = 1;\n"
          "const b: i32 = 2;\n"
          "fn test() -> i32\n"
          "{\n"
          "    while(a > 0 && b == 2) {\n"
          "        if(a < 2)\n"
          "        {\n"
          "            return 1;\n"
          "        }\n"
          "        return 2;\n"
          "    }\n"
          "    return 0;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        co::context co_ctx{sema_env};
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        cg::context ctx{sema_env, const_env, lowering_ctx};
        slang::cfg::simplify cfg_context{ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->bind_constant_declarations(sema_env, const_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->evaluate_constant_expressions(type_ctx, const_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));
        ASSERT_NO_THROW(cfg_context.run());

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "0:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(opt_cfg, if_while_empty_blocks)
{
    {
        const std::string test_input =
          "fn test() -> i32\n"
          "{\n"
          "    if(0) {\n"
          "        return 2;\n"
          "    }\n"
          "    while(0) {\n"
          "        return 1;\n"
          "    }\n"
          "    return 0;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        co::context co_ctx{sema_env};
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        cg::context ctx{sema_env, const_env, lowering_ctx};
        slang::cfg::simplify cfg_context{ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->bind_constant_declarations(sema_env, const_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->evaluate_constant_expressions(type_ctx, const_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));
        ASSERT_NO_THROW(cfg_context.run());

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "entry:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(opt_cfg, while_merge_blocks)
{
    {
        const std::string test_input =
          "fn test() -> i32\n"
          "{\n"
          "    let a: i32 = 1;\n"
          "    while(1) {\n"
          "        a = a + 1;\n"
          "        break;\n"
          "    }\n"
          "    return 0;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        sema::env sema_env;
        const_::env const_env;
        macro::env macro_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        co::context co_ctx{sema_env};
        rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
        cg::context ctx{sema_env, const_env, lowering_ctx};
        slang::cfg::simplify cfg_context{ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->bind_constant_declarations(sema_env, const_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->evaluate_constant_expressions(type_ctx, const_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));
        ASSERT_NO_THROW(cfg_context.run());

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " const i32 1\n"
                  " add i32\n"
                  " store i32 %1\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
}

}    // namespace
