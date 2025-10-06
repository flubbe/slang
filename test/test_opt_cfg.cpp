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

#include "compiler/codegen/codegen.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "compiler/opt/cfg.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace const_ = slang::const_;
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
    ctx.evaluate_constant_subexpressions = false;
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
    ty::context type_ctx;
    co::context co_ctx{sema_env};
    rs::context resolver_ctx{sema_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx = get_context(sema_env, const_env, lowering_ctx);
    slang::opt::cfg::context cfg_context{codegen_ctx};

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
local i32 %i
entry:
 const i32 12
 ret i32
})");
}

}    // namespace
