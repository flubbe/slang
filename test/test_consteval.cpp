/**
 * slang - a simple scripting language.
 *
 * Compile-time evaluation tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>
#include <utility>

#include <gtest/gtest.h>

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
namespace tl = slang::lowering;
namespace ty = slang::typing;

namespace
{

TEST(const_eval, return_statement)
{
    const std::string test_input =
      "const i: f32 = 1.2f32;\n"
      "const a: i32 = 1;\n"
      "const b: i32 = 2;\n"
      "fn test() -> i32\n"
      "{\n"
      "    return (a > 0) && (b < 0);\n"
      "}\n"
      "fn main(args: str[]) -> i32\n"
      "{\n"
      "    return 1.0 as i32;\n"
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

    EXPECT_EQ(ctx.to_string(),
              "define i32 @test() {\n"
              "entry:\n"
              " const i32 0\n"
              " ret i32\n"
              "}\n"
              "define i32 @main(ref %5) {\n"
              "entry:\n"
              " const i32 1\n"
              " ret i32\n"
              "}");
}

TEST(const_eval, disable_const_eval)
{
    const std::string test_input =
      "const i: f32 = 1.2f32;\n"
      "const a: i32 = 1;\n"
      "const b: i32 = 2;\n"
      "fn main(args: str[]) -> i32\n"
      "{\n"
      "    #[disable(const_eval)]\n"
      "    return 1.0 as i32;\n"
      "}\n"
      "fn test() -> i32\n"
      "{\n"
      "    return (a > 0) && (b < 0);\n"
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

    EXPECT_EQ(ctx.to_string(),
              "define i32 @main(ref %4) {\n"
              "entry:\n"
              " const f64 1\n"
              " cast f64_to_i32\n"
              " ret i32\n"
              "}\n"
              "define i32 @test() {\n"
              "entry:\n"
              " const i32 0\n"
              " ret i32\n"
              "}");
}

TEST(const_eval, if_statement)
{
    {
        const std::string test_input =
          "const i: f32 = 1.2f32;\n"
          "const a: i32 = 1;\n"
          "const b: i32 = 2;\n"
          "fn test() -> i32\n"
          "{\n"
          "    if((a > 0) && (b < 0))\n"
          "    {\n"
          "        return 1;\n"
          "    }\n"
          "    else\n"
          "    {\n"
          "        return 0;\n"
          "    }\n"
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

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "entry:\n"
                  " const i32 0\n"
                  " jnz %0, %2\n"
                  "0:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "2:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
    {
        // disable const_eval for if-statement

        const std::string test_input =
          "const i: f32 = 1.2f32;\n"
          "const a: i32 = 1;\n"
          "const b: i32 = 2;\n"
          "fn test() -> i32\n"
          "{\n"
          "    #[disable(const_eval)]\n"
          "    if((a > 0) && (b < 0))\n"
          "    {\n"
          "        return 1+2;\n"
          "    }\n"
          "    else\n"
          "    {\n"
          "        return 0+1;\n"
          "    }\n"
          "    return 2+3;\n"
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

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 0\n"
                  " cmpg i32\n"
                  " const i32 0\n"
                  " cmpne i32\n"
                  " jnz %0, %1\n"
                  "0:\n"
                  " const i32 2\n"
                  " const i32 0\n"
                  " cmpl i32\n"
                  " const i32 0\n"
                  " cmpne i32\n"
                  " jmp %2\n"
                  "1:\n"
                  " const i32 0\n"
                  " jmp %2\n"
                  "2:\n"
                  " jnz %3, %5\n"
                  "3:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " add i32\n"
                  " ret i32\n"
                  "5:\n"
                  " const i32 0\n"
                  " const i32 1\n"
                  " add i32\n"
                  " ret i32\n"
                  "6:\n"
                  " const i32 5\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(const_eval, while_statement)
{
    {
        const std::string test_input =
          "const a: i32 = 1;\n"
          "const b: i32 = 2;\n"
          "fn test() -> i32\n"
          "{\n"
          "    while(a < 2 && b > 0) {\n"
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

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "entry:\n"
                  "0:\n"
                  " const i32 1\n"
                  " jnz %1, %2\n"
                  "1:\n"
                  " jmp %2\n"
                  " jmp %0\n"
                  "2:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "const a: i32 = 1;\n"
          "fn test() -> i32\n"
          "{\n"
          "    #[disable(const_eval)]\n"
          "    while(a < 2) {\n"
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

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test() {\n"
                  "entry:\n"
                  "0:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " cmpl i32\n"
                  " jnz %1, %2\n"
                  "1:\n"
                  " jmp %2\n"
                  " jmp %0\n"
                  "2:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
}

}    // namespace
