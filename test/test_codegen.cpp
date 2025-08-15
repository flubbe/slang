/**
 * slang - a simple scripting language.
 *
 * Code generation tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <memory>

#include <gtest/gtest.h>

#include "compiler/codegen.h"
#include "compiler/sema.h"
#include "compiler/typing.h"

namespace cg = slang::codegen;
namespace const_ = slang::const_;
namespace sema = slang::sema;
namespace tl = slang::lowering;
namespace ty = slang::typing;

namespace
{

// Mock type ids.
constexpr ty::type_id mock_null_type = 0;
constexpr ty::type_id mock_void_type = 1;
constexpr ty::type_id mock_i32_type = 2;
constexpr ty::type_id mock_f32_type = 3;
constexpr ty::type_id mock_str_type = 4;

TEST(codegen, initialize_context)
{
    sema::env sema_env;
    const_::env const_env;
    ty::context type_ctx;
    tl::context lowering_ctx{type_ctx};
    ASSERT_NO_THROW(cg::context(sema_env, const_env, lowering_ctx));
}

TEST(codegen, create_function)
{
    sema::env sema_env;
    const_::env const_env;
    ty::context type_ctx;
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

    auto* fn = codegen_ctx.create_function(
      "test",
      std::make_unique<cg::value>(cg::type{
        mock_void_type, cg::type_kind::void_}),
      {});
    ASSERT_NE(fn, nullptr);

    cg::basic_block* fn_block = cg::basic_block::create(codegen_ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    auto* other_fn = codegen_ctx.create_function(
      "test2",
      std::make_unique<cg::value>(cg::type{
        mock_i32_type, cg::type_kind::i32}),
      {});
    ASSERT_NE(other_fn, nullptr);
    ASSERT_NE(fn, other_fn);

    cg::basic_block* other_fn_block = cg::basic_block::create(codegen_ctx, "entry");
    fn->append_basic_block(other_fn_block);
    ASSERT_NE(other_fn_block, nullptr);

    ASSERT_NE(fn_block, other_fn_block);

    EXPECT_THROW(static_cast<void>(    // ignore return value, since the function should throw anyway.
                   codegen_ctx.create_function(
                     "test",
                     std::make_unique<cg::value>(
                       cg::type{
                         mock_i32_type, cg::type_kind::i32}),
                     {})),
                 cg::codegen_error);
}

TEST(codegen, insertion_points)
{
    sema::env sema_env;
    const_::env const_env;
    ty::context type_ctx;
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

    auto* fn = codegen_ctx.create_function(
      "test",
      std::make_unique<cg::value>(cg::type{
        mock_void_type, cg::type_kind::void_}),
      {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = cg::basic_block::create(codegen_ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    codegen_ctx.set_insertion_point(fn_block);
    EXPECT_EQ(codegen_ctx.get_insertion_point(), fn_block);

    // scoped context.
    auto* block = cg::basic_block::create(codegen_ctx, "outer");
    {
        auto inner_ctx = cg::context{sema_env, const_env, lowering_ctx};
        inner_ctx.set_insertion_point(block);

        EXPECT_EQ(inner_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &inner_ctx);
    }
    EXPECT_EQ(block->get_inserting_context(), nullptr);
}

TEST(codegen, validate_basic_block)
{
    sema::env sema_env;
    const_::env const_env;
    ty::context type_ctx;
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

    auto* fn = codegen_ctx.create_function(
      "test",
      std::make_unique<cg::value>(cg::type{
        mock_void_type, cg::type_kind::void_}),
      {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = cg::basic_block::create(codegen_ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    codegen_ctx.set_insertion_point(fn_block);
    EXPECT_EQ(codegen_ctx.get_insertion_point(), fn_block);

    EXPECT_FALSE(fn_block->is_valid());

    codegen_ctx.generate_ret();

    EXPECT_TRUE(fn_block->is_valid());

    codegen_ctx.generate_branch(fn_block);

    EXPECT_FALSE(fn_block->is_valid());
}

TEST(codegen, generate_function)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> void
         * {
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(cg::type{
            mock_void_type, cg::type_kind::void_}),
          std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define void @f(i32 %a) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     return -31;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}),
          std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_const(
          {cg::type{
            mock_i32_type, cg::type_kind::i32}},
          -31);    // NOLINT(readability-magic-numbers)
        codegen_ctx.generate_ret(
          std::make_optional<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " const i32 -31\n"
                  " ret i32\n"
                  "}");
    }
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}),
          std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_load(
          std::make_unique<cg::variable_argument>(
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type, cg::type_kind::i32},
              "a")));
        codegen_ctx.generate_ret(
          std::make_optional<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(codegen, operators)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a + 1;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}),
          std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_load(
          std::make_unique<cg::variable_argument>(
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type, cg::type_kind::i32},
              "a")));
        codegen_ctx.generate_const(
          {cg::type{
            mock_i32_type, cg::type_kind::i32}},
          1);
        codegen_ctx.generate_binary_op(
          cg::binary_op::op_add,
          {cg::type{
            mock_i32_type, cg::type_kind::i32}});

        codegen_ctx.generate_ret(
          std::make_optional<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " const i32 1\n"
                  " add i32\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(codegen, conditional_branch)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     if(a == 1)
         *     {
         *         return 1;
         *     }
         *     return 0;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32}),
          std::move(args));

        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* cond = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(cond);
        ASSERT_NE(cond, nullptr);
        EXPECT_EQ(cond->get_inserting_context(), nullptr);
        EXPECT_EQ(cond->get_label(), "entry");

        cg::basic_block* then_block = cg::basic_block::create(codegen_ctx, "then");
        cg::basic_block* else_block = cg::basic_block::create(codegen_ctx, "else");
        cg::basic_block* cont_block = cg::basic_block::create(codegen_ctx, "cont");

        fn->append_basic_block(then_block);
        fn->append_basic_block(else_block);
        fn->append_basic_block(cont_block);

        codegen_ctx.set_insertion_point(cond);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), cond);

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));
        codegen_ctx.generate_const({cg::type{mock_i32_type, cg::type_kind::i32}}, 1);
        codegen_ctx.generate_binary_op(cg::binary_op::op_equal, {cg::type{mock_i32_type, cg::type_kind::i32}});
        codegen_ctx.generate_cond_branch(then_block, else_block);

        codegen_ctx.set_insertion_point(then_block);
        codegen_ctx.generate_const({cg::type{mock_i32_type, cg::type_kind::i32}}, 1);
        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        codegen_ctx.set_insertion_point(else_block);
        codegen_ctx.generate_branch(cont_block);

        codegen_ctx.set_insertion_point(cont_block);
        codegen_ctx.generate_const({cg::type{mock_i32_type, cg::type_kind::i32}}, 0);
        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(then_block->is_valid());
        EXPECT_TRUE(else_block->is_valid());
        EXPECT_TRUE(cont_block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " const i32 1\n"
                  " cmpeq i32\n"
                  " jnz %then, %else\n"
                  "then:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "else:\n"
                  " jmp %cont\n"
                  "cont:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(codegen, locals_store)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> void
         * {
         *     let b: i32 = a;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        auto* fn = codegen_ctx.create_function(
          "test",
          std::make_unique<cg::value>(cg::type{
            mock_void_type, cg::type_kind::void_}),
          std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        fn->create_local(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b"));

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));
        codegen_ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));

        codegen_ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define void @f(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " store i32 %b\n"
                  " ret void\n"
                  "}");
    }
}

TEST(codegen, invoke)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     let b: i32 = -1;
         *     return g(b, a);
         * }
         *
         * fn g(a: i32, b:i32) -> i32
         * {
         *     return a*b;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args_f;
        args_f.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a"));

        auto* fn_f = codegen_ctx.create_function("f", std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}), std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b"));

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_const({cg::type{mock_i32_type, cg::type_kind::i32}}, -1);
        codegen_ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));
        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));

        // FIXME Needs sema env set up.
        const slang::sema::symbol_id mock_symbol_id{0};

        codegen_ctx.generate_invoke(
          std::make_unique<cg::function_argument>(mock_symbol_id));

        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a"));
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b"));

        auto* fn_g = codegen_ctx.create_function("g", std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}), std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = cg::basic_block::create(codegen_ctx, "entry");
        fn_g->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));
        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));
        codegen_ctx.generate_binary_op(cg::binary_op::op_mul, {cg::type{mock_i32_type, cg::type_kind::i32}});

        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " const i32 -1\n"
                  " store i32 %b\n"
                  " load i32 %b\n"
                  " load i32 %a\n"
                  " invoke @g\n"
                  " ret i32\n"
                  "}\n"
                  "define i32 @g(i32 %a, i32 %b) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " load i32 %b\n"
                  " mul i32\n"
                  " ret i32\n"
                  "}");
    }
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f(a: i32) -> i32
         * {
         *     let b: i32 = -1;
         *     return g(b, a);
         * }
         *
         * fn g(a: i32, b: i32) -> i32
         * {
         *     return a*b;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args_f;
        args_f.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a"));

        auto* fn_f = codegen_ctx.create_function("f", std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}), std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b"));

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_const({cg::type{mock_i32_type, cg::type_kind::i32}}, -1);
        codegen_ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));
        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));

        // FIXME Needs sema env set up.
        const slang::sema::symbol_id mock_symbol_id{0};

        codegen_ctx.generate_load(
          std::make_unique<cg::function_argument>(
            mock_symbol_id));
        codegen_ctx.generate_invoke();

        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a"));
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b"));

        auto* fn_g = codegen_ctx.create_function("g", std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}), std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = cg::basic_block::create(codegen_ctx, "entry");
        fn_g->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "a")));
        codegen_ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}, "b")));
        codegen_ctx.generate_binary_op(cg::binary_op::op_mul, {cg::type{mock_i32_type, cg::type_kind::i32}});

        codegen_ctx.generate_ret(std::make_optional<cg::value>(cg::type{mock_i32_type, cg::type_kind::i32}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " const i32 -1\n"
                  " store i32 %b\n"
                  " load i32 %b\n"
                  " load i32 %a\n"
                  " load @g\n"
                  " invoke_dynamic\n"
                  " ret i32\n"
                  "}\n"
                  "define i32 @g(i32 %a, i32 %b) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " load i32 %b\n"
                  " mul i32\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(codegen, struct_type)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * struct S
         * {
         *     a: i32,
         *     b: f32
         * };
         *
         * fn f() -> i32
         * {
         *     let s: S = S{1, 2};
         *     return s.a;
         * }
         */

        codegen_ctx.add_struct("S", {{"a", {cg::type{mock_i32_type, cg::type_kind::i32}}}, {"b", {cg::type{mock_i32_type, cg::type_kind::i32}}}});

        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(
          std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            "a"));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %a,\n"
                  " i32 %b,\n"
                  "}");
    }
}

TEST(codegen, strings)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        /*
         * fn f() -> str
         * {
         *     return "\tTest\n";
         * }
         */

        auto* fn_f = codegen_ctx.create_function(
          "f",
          std::make_unique<cg::value>(
            cg::type{mock_str_type, cg::type_kind::str}),
          {});
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_const(
          {cg::type{mock_str_type, cg::type_kind::str}},
          "\tTest\n");
        codegen_ctx.generate_ret(
          std::make_optional<cg::value>(
            cg::type{mock_str_type, cg::type_kind::str}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  ".string @0 \"\\x09Test\\x0a\"\n"
                  "define str @f() {\n"
                  "entry:\n"
                  " const str @0\n"
                  " ret str\n"
                  "}");
    }
}

}    // namespace
