/**
 * slang - a simple scripting language.
 *
 * Code generation tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "codegen.h"

namespace cg = slang::codegen;

namespace
{

TEST(codegen, initialize_context)
{
    cg::context ctx = cg::context();
}

TEST(codegen, create_function)
{
    cg::context ctx = cg::context();
    auto fn = ctx.create_function("test", "void", {});
    ASSERT_NE(fn, nullptr);

    cg::basic_block* fn_block = fn->create_basic_block("entry");
    ASSERT_NE(fn_block, nullptr);

    auto other_fn = ctx.create_function("test2", "i32", {});
    ASSERT_NE(other_fn, nullptr);
    ASSERT_NE(fn, other_fn);

    cg::basic_block* other_fn_block = fn->create_basic_block("entry");
    ASSERT_NE(other_fn_block, nullptr);

    ASSERT_NE(fn_block, other_fn_block);

    EXPECT_THROW(ctx.create_function("test", "i32", {}), cg::codegen_error);
}

TEST(codegen, insertion_points)
{
    auto ctx = cg::context();
    auto fn = ctx.create_function("test", "void", {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = fn->create_basic_block("entry");
    ASSERT_NE(fn_block, nullptr);

    ctx.set_insertion_point(fn_block);
    EXPECT_EQ(ctx.get_insertion_point(), fn_block);

    // scoped basic block.
    {
        auto scoped_block = cg::basic_block("scope");
        ctx.set_insertion_point(&scoped_block);
        EXPECT_EQ(ctx.get_insertion_point(), &scoped_block);
        EXPECT_EQ(scoped_block.get_inserting_context(), &ctx);
    }
    EXPECT_EQ(ctx.get_insertion_point(), nullptr);

    // scoped context.
    auto block = cg::basic_block("outer");
    {
        auto inner_ctx = cg::context();
        inner_ctx.set_insertion_point(&block);

        EXPECT_EQ(inner_ctx.get_insertion_point(), &block);
        EXPECT_EQ(block.get_inserting_context(), &inner_ctx);
    }
    EXPECT_EQ(block.get_inserting_context(), nullptr);
}

TEST(codegen, validate_basic_block)
{
    auto ctx = cg::context();
    auto fn = ctx.create_function("test", "void", {});
    ASSERT_NE(fn, nullptr);

    // basic block created by function.
    cg::basic_block* fn_block = fn->create_basic_block("entry");
    ASSERT_NE(fn_block, nullptr);

    ctx.set_insertion_point(fn_block);
    EXPECT_EQ(ctx.get_insertion_point(), fn_block);

    EXPECT_FALSE(fn_block->is_valid());

    ctx.generate_ret();

    EXPECT_TRUE(fn_block->is_valid());

    ctx.generate_branch(std::make_unique<cg::label_argument>("some_label"));

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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "void", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = fn->create_basic_block("entry");
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %a) {\n"
                  "entry:\n"
                  " ret\n"
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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "i32", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = fn->create_basic_block("entry");
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_const({"i32"}, -31);
        ctx.generate_ret(std::make_optional<cg::value>("i32"));

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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "i32", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = fn->create_basic_block("entry");
        ASSERT_NE(block, nullptr);

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_ret(std::make_optional<cg::value>("i32"));

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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "i32", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = fn->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_const({"i32"}, 1);
        ctx.generate_binary_op(cg::binary_op::op_add, {"i32"});

        ctx.generate_ret(std::make_optional<cg::value>("i32"));

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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "i32", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* cond = fn->create_basic_block("entry");
        ASSERT_NE(cond, nullptr);
        EXPECT_EQ(cond->get_inserting_context(), nullptr);
        EXPECT_EQ(cond->get_label(), "entry");

        auto then_block = fn->create_basic_block("then");
        auto else_block = fn->create_basic_block("else");
        auto cont_block = fn->create_basic_block("cont");

        ctx.set_insertion_point(cond);
        EXPECT_EQ(ctx.get_insertion_point(), cond);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_const({"i32"}, 1);
        ctx.generate_cmp();
        ctx.generate_cond_branch(then_block, else_block);

        ctx.set_insertion_point(then_block);
        ctx.generate_const({"i32"}, 1);
        ctx.generate_ret(std::make_optional<cg::value>("i32"));

        ctx.set_insertion_point(else_block);
        ctx.generate_branch(std::make_unique<cg::label_argument>("cont"));

        ctx.set_insertion_point(cont_block);
        ctx.generate_const({"i32"}, 0);
        ctx.generate_ret(std::make_optional<cg::value>("i32"));

        EXPECT_TRUE(then_block->is_valid());
        EXPECT_TRUE(else_block->is_valid());
        EXPECT_TRUE(cont_block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %a) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " const i32 1\n"
                  " cmp\n"
                  " ifeq %then, %else\n"
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
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn = ctx.create_function("f", "void", std::move(args));
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        fn->create_local(std::make_unique<cg::value>("i32", std::nullopt, "b"));

        cg::basic_block* block = fn->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_store(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));

        ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " store i32 %b\n"
                  " ret\n"
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
        args_f.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn_f = ctx.create_function("f", "i32", std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>("i32", std::nullopt, "b"));

        cg::basic_block* block = fn_f->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({"i32"}, -1);
        ctx.generate_store(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));

        ctx.generate_invoke(std::make_unique<cg::function_argument>("g"));

        ctx.generate_ret(std::make_optional<cg::value>("i32"));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));
        args_g.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "b"));

        auto fn_g = ctx.create_function("g", "i32", std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = fn_g->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));
        ctx.generate_binary_op(cg::binary_op::op_mul, {"i32"});

        ctx.generate_ret(std::make_optional<cg::value>("i32"));

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
        args_f.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn_f = ctx.create_function("f", "i32", std::move(args_f));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>("i32", std::nullopt, "b"));

        cg::basic_block* block = fn_f->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({"i32"}, -1);
        ctx.generate_store(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));

        ctx.generate_load(std::make_unique<cg::function_argument>("g"));
        ctx.generate_invoke();

        ctx.generate_ret(std::make_optional<cg::value>("i32"));

        EXPECT_TRUE(block->is_valid());

        std::vector<std::unique_ptr<cg::value>> args_g;
        args_g.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));
        args_g.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "b"));

        auto fn_g = ctx.create_function("g", "i32", std::move(args_g));
        ASSERT_NE(fn_g, nullptr);
        EXPECT_EQ(fn_g->get_name(), "g");

        block = fn_g->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "a"}));
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"i32", std::nullopt, "b"}));
        ctx.generate_binary_op(cg::binary_op::op_mul, {"i32"});

        ctx.generate_ret(std::make_optional<cg::value>("i32"));

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

TEST(codegen, aggregate_data)
{
    {
        cg::context ctx = cg::context();

        /*
         *
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

        ctx.create_type("S", {{"a", "i32"}, {"b", "i32"}});

        std::vector<std::unique_ptr<cg::value>> args;
        args.emplace_back(std::make_unique<cg::value>("i32", std::nullopt, "a"));

        auto fn_f = ctx.create_function("f", "i32", std::move(args));
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        fn_f->create_local(std::make_unique<cg::value>("aggregate", "S", "s"));

        cg::basic_block* block = fn_f->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({"i32"}, 1);
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"addr", std::nullopt, "s"}));
        ctx.generate_store_element({0});

        ctx.generate_const({"i32"}, 2);
        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"addr", std::nullopt, "s"}));
        ctx.generate_store_element({1});

        ctx.generate_load(std::make_unique<cg::variable_argument>(cg::value{"addr", std::nullopt, "s"}));
        ctx.generate_load_element({0});
        ctx.generate_ret(std::make_optional<cg::value>("i32"));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %a,\n"
                  " i32 %b,\n"
                  "}\n"
                  "define i32 @f(i32 %a) {\n"
                  "local S %s\n"
                  "entry:\n"
                  " const i32 1\n"
                  " load addr %s\n"
                  " store_element i32 0\n"
                  " const i32 2\n"
                  " load addr %s\n"
                  " store_element i32 1\n"
                  " load addr %s\n"
                  " load_element i32 0\n"
                  " ret i32\n"
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

        auto fn_f = ctx.create_function("f", "str", {});
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        cg::basic_block* block = fn_f->create_basic_block("entry");
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        ctx.set_insertion_point(block);
        EXPECT_EQ(ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &ctx);

        ctx.generate_const({"str"}, "\tTest\n");
        ctx.generate_ret(std::make_optional<cg::value>("str"));

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
