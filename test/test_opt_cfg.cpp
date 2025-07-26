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

#include "compiler/codegen.h"
#include "compiler/parser.h"
#include "compiler/typing.h"
#include "compiler/opt/cfg.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace sema = slang::sema;
namespace ty = slang::typing;

namespace
{

cg::context get_context()
{
    cg::context ctx;
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

    ty::context type_ctx;
    cg::context codegen_ctx = get_context();
    sema::env env;
    co::context co_ctx{env};
    slang::opt::cfg::context cfg_context{codegen_ctx};

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(type_ctx.resolve_types());
    ASSERT_NO_THROW(ast->type_check(type_ctx));
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
