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

namespace cg = slang::codegen;

namespace
{

TEST(codegen, initialize_context)
{
    ASSERT_NO_THROW(cg::context ctx = cg::context());
}

TEST(codegen, create_function)
{
    cg::context ctx = cg::context();
    auto* fn = ctx.create_function("test", std::make_unique<cg::value>(cg::type{cg::type_class::void_, 0}), {});
    ASSERT_NE(fn, nullptr);

    cg::basic_block* fn_block = cg::basic_block::create(ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    auto* other_fn = ctx.create_function("test2", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), {});
    ASSERT_NE(other_fn, nullptr);
    ASSERT_NE(fn, other_fn);

    cg::basic_block* other_fn_block = cg::basic_block::create(ctx, "entry");
    fn->append_basic_block(other_fn_block);
    ASSERT_NE(other_fn_block, nullptr);

    ASSERT_NE(fn_block, other_fn_block);

    EXPECT_THROW(static_cast<void>(    // ignore return value, since the function should throw anyway.
                   ctx.create_function(
                     "test",
                     std::make_unique<cg::value>(
                       cg::type{
                         cg::type_class::i32, 0}),
                     {})),
                 cg::codegen_error);
}

TEST(codegen, insertion_points)
{
    auto ctx = cg::context();
    auto* fn = ctx.create_function("test", std::make_unique<cg::value>(cg::type{cg::type_class::void_, 0}), {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = cg::basic_block::create(ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    ctx.set_insertion_point(fn_block);
    EXPECT_EQ(ctx.get_insertion_point(), fn_block);

    // scoped context.
    auto* block = cg::basic_block::create(ctx, "outer");
    {
        auto inner_ctx = cg::context();
        inner_ctx.set_insertion_point(block);

        EXPECT_EQ(inner_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &inner_ctx);
    }
    EXPECT_EQ(block->get_inserting_context(), nullptr);
}

TEST(codegen, validate_basic_block)
{
    auto ctx = cg::context();
    auto* fn = ctx.create_function("test", std::make_unique<cg::value>(cg::type{cg::type_class::void_, 0}), {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = cg::basic_block::create(ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    ctx.set_insertion_point(fn_block);
    EXPECT_EQ(ctx.get_insertion_point(), fn_block);

    EXPECT_FALSE(fn_block->is_valid());

    ctx.generate_ret();

    EXPECT_TRUE(fn_block->is_valid());

    ctx.generate_branch(fn_block);

    EXPECT_FALSE(fn_block->is_valid());
}

TEST(codegen, generate_function)
{
    {
        cg::context ctx = cg::context();

        /*
         * fn f(a: i32) -> void
         * {
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::void_, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %a) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        cg::context ctx = cg::context();

        /*
         * fn f(a: i32) -> i32
         * {
         *     return -31;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, -31);    // NOLINT(readability-magic-numbers)
        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " const i32 -31\n"
                  " ret i32\n"
                  "}");
    }
    {
        cg::context ctx = cg::context();

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a + 1;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 1);
        ctx.generate_binary_op(cg::binary_op::op_add, {cg::type{cg::type_class::i32, 0}});

        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

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
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* cond = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(cond);
        ASSERT_NE(cond, nullptr);
        EXPECT_EQ(cond->get_inserting_context(), nullptr);
        EXPECT_EQ(cond->get_label(), "entry");

        cg::basic_block* then_block = cg::basic_block::create(ctx, "then");
        cg::basic_block* else_block = cg::basic_block::create(ctx, "else");
        cg::basic_block* cont_block = cg::basic_block::create(ctx, "cont");

        fn->append_basic_block(then_block);
        fn->append_basic_block(else_block);
        fn->append_basic_block(cont_block);

        ctx.set_insertion_point(cond);
        EXPECT_EQ(ctx.get_insertion_point(), cond);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 1);
        ctx.generate_binary_op(cg::binary_op::op_equal, {cg::type{cg::type_class::i32, 0}});
        ctx.generate_cond_branch(then_block, else_block);

        ctx.set_insertion_point(then_block);
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 1);
        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        ctx.set_insertion_point(else_block);
        ctx.generate_branch(cont_block);

        ctx.set_insertion_point(cont_block);
        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, 0);
        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(then_block->is_valid());
        EXPECT_TRUE(else_block->is_valid());
        EXPECT_TRUE(cont_block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

        /*
         * fn f(a: i32) -> void
         * {
         *     let b: i32 = a;
         * }
         */
        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::void_, 0}), std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        fn->create_local(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b"));

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));

        ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

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
        args_f.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn_f = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b"));

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, -1);
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));
        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));

        ctx.generate_invoke(std::make_unique<cg::function_argument>("g", std::nullopt));

        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b"));

        auto* fn_g = ctx.create_function("g", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = cg::basic_block::create(ctx, "entry");
        fn_g->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));
        ctx.generate_binary_op(cg::binary_op::op_mul, {cg::type{cg::type_class::i32, 0}});

        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

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
        args_f.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        auto* fn_f = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b"));

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({cg::type{cg::type_class::i32, 0}}, -1);
        ctx.generate_store(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));
        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));

        ctx.generate_load(std::make_unique<cg::function_argument>("g", std::nullopt));
        ctx.generate_invoke();

        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));
        args_g.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b"));

        auto* fn_g = ctx.create_function("g", std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}), std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = cg::basic_block::create(ctx, "entry");
        fn_g->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a")));
        ctx.generate_load(std::make_unique<cg::variable_argument>(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "b")));
        ctx.generate_binary_op(cg::binary_op::op_mul, {cg::type{cg::type_class::i32, 0}});

        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::i32, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = cg::context();

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

        ctx.add_struct("S", {{"a", {cg::type{cg::type_class::i32, 0}}}, {"b", {cg::type{cg::type_class::i32, 0}}}});

        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>(cg::type{cg::type_class::i32, 0}, "a"));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %a,\n"
                  " i32 %b,\n"
                  "}");
    }
}

TEST(codegen, strings)
{
    {
        cg::context ctx = cg::context();

        /*
         * fn f() -> str
         * {
         *     return "\tTest\n";
         * }
         */

        auto* fn_f = ctx.create_function("f", std::make_unique<cg::value>(cg::type{cg::type_class::str, 0}), {});
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({cg::type{cg::type_class::str, 0}}, "\tTest\n");
        ctx.generate_ret(std::make_optional<cg::value>(cg::type{cg::type_class::str, 0}));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  ".string @0 \"\\x09Test\\x0a\"\n"
                  "define str @f() {\n"
                  "entry:\n"
                  " const str @0\n"
                  " ret str\n"
                  "}");
    }
}

}    // namespace
