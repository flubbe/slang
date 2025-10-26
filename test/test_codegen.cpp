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

#include "compiler/codegen/codegen.h"
#include "compiler/collect.h"
#include "compiler/name_utils.h"
#include "compiler/sema.h"
#include "compiler/typing.h"

namespace cg = slang::codegen;
namespace co = slang::collect;
namespace const_ = slang::const_;
namespace sema = slang::sema;
namespace tl = slang::lowering;
namespace ty = slang::typing;

namespace
{

// Mock type ids.
// Correspond to the type id's in the lowering context's builtin initialization.

// constexpr ty::type_id mock_null_type = 0;
constexpr ty::type_id mock_void_type = 1;
constexpr ty::type_id mock_i32_type = 2;
// constexpr ty::type_id mock_f32_type = 3;
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
      cg::type{
        mock_void_type,
        cg::type_kind::void_},
      {});
    ASSERT_NE(fn, nullptr);

    cg::basic_block* fn_block = cg::basic_block::create(codegen_ctx, "entry");
    fn->append_basic_block(fn_block);
    ASSERT_NE(fn_block, nullptr);

    auto* other_fn = codegen_ctx.create_function(
      "test2",
      cg::type{
        mock_i32_type,
        cg::type_kind::i32},
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
                     cg::type{
                       mock_i32_type, cg::type_kind::i32},
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
      cg::type{
        mock_void_type, cg::type_kind::void_},
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
      cg::type{
        mock_void_type, cg::type_kind::void_},
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

/**
 * Helper to add an argument to a function.
 *
 * @param ctx The collection context associated to a semantic environment.
 * @param arg The function's argument list to add the argument to.
 * @param name The argument's name.
 * @param ty The argument's type.
 */
static void add_arg(
  co::context& ctx,
  std::vector<
    std::pair<
      sema::symbol_id,
      cg::type>>&
    args,
  const std::string& name,
  cg::type ty)
{
    auto id = ctx.declare(
      name,
      name,
      sema::symbol_type::argument,
      {0, 0},
      sema::symbol_id::invalid,
      false);

    args.emplace_back(id, ty);
}

TEST(codegen, generate_function)
{
    {
        sema::env sema_env;
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f(a: i32) -> void
         * {
         * }
         */
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;

        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_void_type, cg::type_kind::void_},
          args);
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
                  "define void @f(i32 %0) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        sema::env sema_env;
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f(a: i32) -> i32
         * {
         *     return -31;
         * }
         */
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;
        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type, cg::type_kind::i32},
          args);
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_const(
          {mock_i32_type, cg::type_kind::i32},
          -31);    // NOLINT(readability-magic-numbers)
        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type, cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %0) {\n"
                  "entry:\n"
                  " const i32 -31\n"
                  " ret i32\n"
                  "}");
    }
    {
        sema::env sema_env;
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a;
         * }
         */
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;
        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type, cg::type_kind::i32},
          args);
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_load(
          cg::variable_argument{std::make_unique<cg::value>(
            cg::type{
              mock_i32_type, cg::type_kind::i32},
            args[0].first)});
        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type, cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %0) {\n"
                  "entry:\n"
                  " load i32 %0\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(codegen, operators)
{
    {
        sema::env sema_env;
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f(a: i32) -> i32
         * {
         *     return a + 1;
         * }
         */
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;
        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type, cg::type_kind::i32},
          args);
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
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type, cg::type_kind::i32},
              args[0].first)});
        codegen_ctx.generate_const(
          {mock_i32_type, cg::type_kind::i32},
          1);
        codegen_ctx.generate_binary_op(
          cg::binary_op::op_add,
          {mock_i32_type, cg::type_kind::i32});

        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type, cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %0) {\n"
                  "entry:\n"
                  " load i32 %0\n"
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
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

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
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;
        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type, cg::type_kind::i32},
          args);

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

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{mock_i32_type, cg::type_kind::i32},
              args[0].first)});
        codegen_ctx.generate_const({mock_i32_type, cg::type_kind::i32}, 1);
        codegen_ctx.generate_binary_op(cg::binary_op::op_equal, {mock_i32_type, cg::type_kind::i32});
        codegen_ctx.generate_cond_branch(then_block, else_block);

        codegen_ctx.set_insertion_point(then_block);
        codegen_ctx.generate_const({mock_i32_type, cg::type_kind::i32}, 1);
        codegen_ctx.generate_ret(std::make_optional<cg::type>(mock_i32_type, cg::type_kind::i32));

        codegen_ctx.set_insertion_point(else_block);
        codegen_ctx.generate_branch(cont_block);

        codegen_ctx.set_insertion_point(cont_block);
        codegen_ctx.generate_const({mock_i32_type, cg::type_kind::i32}, 0);
        codegen_ctx.generate_ret(std::make_optional<cg::type>(mock_i32_type, cg::type_kind::i32));

        EXPECT_TRUE(then_block->is_valid());
        EXPECT_TRUE(else_block->is_valid());
        EXPECT_TRUE(cont_block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define i32 @f(i32 %0) {\n"
                  "entry:\n"
                  " load i32 %0\n"
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

/**
 * Helper to add a local variable to a function.
 *
 * @param ctx The collection context associated to a semantic environment.
 * @param fn The function to add the local to.
 * @param name The local's name.
 * @param ty The local's type.
 */
static sema::symbol_id add_local(
  co::context& ctx,
  cg::function* fn,
  std::string name,
  cg::type ty)
{
    auto local_id = ctx.declare(
      name,
      slang::name::qualified_name(fn->get_name(), name),
      sema::symbol_type::variable,
      {0, 0},
      sema::symbol_id::invalid,
      false);

    fn->add_local(
      local_id,
      ty);

    return local_id;
}

TEST(codegen, locals_store)
{
    {
        sema::env sema_env;
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f(a: i32) -> void
         * {
         *     let b: i32 = a;
         * }
         */
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args;
        add_arg(
          co_ctx,
          args,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_void_type, cg::type_kind::void_},
          args);
        ASSERT_NE(fn, nullptr);
        EXPECT_EQ(fn->get_name(), "f");

        auto local_id = add_local(
          co_ctx,
          fn,
          "b",
          cg::type{mock_i32_type, cg::type_kind::i32});

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{mock_i32_type, cg::type_kind::i32},
              args[0].first)});
        codegen_ctx.generate_store(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{mock_i32_type, cg::type_kind::i32},
              local_id)});

        codegen_ctx.generate_ret();

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(codegen_ctx.to_string(),
                  "define void @f(i32 %0) {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " load i32 %0\n"
                  " store i32 %1\n"
                  " ret void\n"
                  "}");
    }
}

class test_name_resolver : public cg::name_resolver
{
    /** The semantic environment. */
    sema::env& sema_env;

    /** The type context. */
    ty::context& type_ctx;

public:
    /** Deleted constructors. */
    test_name_resolver() = delete;
    test_name_resolver(const test_name_resolver&) = delete;
    test_name_resolver(const test_name_resolver&&) = delete;

    /**
     * Create a name resolver for the tests.
     *
     * @param sema_env The semantic environment.
     * @param type_ctx The type context.
     */
    test_name_resolver(
      sema::env& sema_env,
      ty::context& type_ctx)
    : sema_env{sema_env}
    , type_ctx{type_ctx}
    {
    }

    /** Default destructor. */
    ~test_name_resolver() override = default;

    /** Deleted assignment operators. */
    test_name_resolver& operator=(const test_name_resolver&) = delete;
    test_name_resolver& operator=(test_name_resolver&&) = delete;

    [[nodiscard]]
    std::string symbol_name(sema::symbol_id id) const override
    {
        auto it = sema_env.symbol_table.find(id);
        if(it == sema_env.symbol_table.end())
        {
            throw std::runtime_error(
              std::format(
                "Symbol with id {} not found in symbol table.",
                id.value));
        }

        return it->second.name;
    }

    [[nodiscard]]
    std::string type_name(ty::type_id id) const override
    {
        return type_ctx.to_string(id);
    }

    [[nodiscard]]
    std::string field_name(
      ty::type_id type_id,    // NOLINT(bugprone-easily-swappable-parameters)
      std::size_t field_index) const override
    {
        auto info = type_ctx.get_struct_info(type_id);
        return info.fields.at(field_index).name;
    }

    [[nodiscard]]
    std::string constant(
      const_::constant_id id) const override
    {
        return {};
    }
};

TEST(codegen, invoke)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        co::context co_ctx{sema_env};
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        test_name_resolver resolver{sema_env, type_ctx};

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
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args_f;
        add_arg(
          co_ctx,
          args_f,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn_f = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type,
            cg::type_kind::i32},
          args_f);
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        auto local_id = add_local(
          co_ctx,
          fn_f,
          "b",
          cg::type{mock_i32_type, cg::type_kind::i32});

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_const(
          {mock_i32_type,
           cg::type_kind::i32},
          -1);
        codegen_ctx.generate_store(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              local_id)});

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              local_id)});
        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_f[0].first)});

        const sema::scope_id mock_scope_id = co_ctx.push_scope(
          std::nullopt,
          {0, 0});

        const sema::symbol_id function_symbol_id = co_ctx.declare(
          "g",
          "g",
          sema::symbol_type::variable,
          {0, 0},
          mock_scope_id,
          false,
          std::nullopt);

        codegen_ctx.generate_invoke(
          cg::function_argument{
            function_symbol_id});

        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type,
            cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args_g;
        add_arg(
          co_ctx,
          args_g,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});
        add_arg(
          co_ctx,
          args_g,
          "b",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn_g = codegen_ctx.create_function(
          "g",
          cg::type{
            mock_i32_type,
            cg::type_kind::i32},
          args_g);
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

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_g[0].first)});
        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_g[1].first)});
        codegen_ctx.generate_binary_op(
          cg::binary_op::op_mul,
          {mock_i32_type,
           cg::type_kind::i32});

        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type,
            cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(
          codegen_ctx.to_string(&resolver),
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
        co::context co_ctx{sema_env};
        const_::env const_env;
        ty::context type_ctx;
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        test_name_resolver resolver{sema_env, type_ctx};

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
        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args_f;
        add_arg(
          co_ctx,
          args_f,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn_f = codegen_ctx.create_function(
          "f",
          cg::type{
            mock_i32_type,
            cg::type_kind::i32},
          args_f);
        ASSERT_NE(fn_f, nullptr);
        EXPECT_EQ(fn_f->get_name(), "f");

        auto local_id = add_local(
          co_ctx,
          fn_f,
          "b",
          cg::type{mock_i32_type, cg::type_kind::i32});

        cg::basic_block* block = cg::basic_block::create(codegen_ctx, "entry");
        fn_f->append_basic_block(block);
        ASSERT_NE(block, nullptr);
        EXPECT_EQ(block->get_inserting_context(), nullptr);
        EXPECT_EQ(block->get_label(), "entry");

        codegen_ctx.set_insertion_point(block);
        EXPECT_EQ(codegen_ctx.get_insertion_point(), block);
        EXPECT_EQ(block->get_inserting_context(), &codegen_ctx);

        codegen_ctx.generate_const(
          {mock_i32_type,
           cg::type_kind::i32},
          -1);
        codegen_ctx.generate_store(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              local_id)});

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              local_id)});
        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_f[0].first)});

        const sema::scope_id mock_scope_id = co_ctx.push_scope(
          std::nullopt,
          {0, 0});

        co_ctx.declare(
          "g",
          "g",
          sema::symbol_type::variable,
          {0, 0},
          mock_scope_id,
          false,
          std::nullopt);

        codegen_ctx.generate_const(
          {mock_i32_type,
           cg::type_kind::i32},
          0);
        codegen_ctx.generate_invoke_dynamic();

        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type,
            cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        std::vector<
          std::pair<
            sema::symbol_id,
            cg::type>>
          args_g;
        add_arg(
          co_ctx,
          args_g,
          "a",
          cg::type{
            mock_i32_type, cg::type_kind::i32});
        add_arg(
          co_ctx,
          args_g,
          "b",
          cg::type{
            mock_i32_type, cg::type_kind::i32});

        auto* fn_g = codegen_ctx.create_function(
          "g",
          cg::type{
            mock_i32_type,
            cg::type_kind::i32},
          args_g);
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

        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_g[0].first)});
        codegen_ctx.generate_load(
          cg::variable_argument{
            std::make_unique<cg::value>(
              cg::type{
                mock_i32_type,
                cg::type_kind::i32},
              args_g[1].first)});
        codegen_ctx.generate_binary_op(
          cg::binary_op::op_mul,
          {mock_i32_type,
           cg::type_kind::i32});

        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_i32_type,
            cg::type_kind::i32));

        EXPECT_TRUE(block->is_valid());

        EXPECT_EQ(
          codegen_ctx.to_string(&resolver),
          "define i32 @f(i32 %a) {\n"
          "local i32 %b\n"
          "entry:\n"
          " const i32 -1\n"
          " store i32 %b\n"
          " load i32 %b\n"
          " load i32 %a\n"
          " const i32 0\n"
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

TEST(codegen, strings)
{
    {
        sema::env sema_env;
        const_::env const_env;
        ty::context type_ctx;
        co::context co_ctx{sema_env};
        tl::context lowering_ctx{type_ctx};
        cg::context codegen_ctx(sema_env, const_env, lowering_ctx);

        co_ctx.push_scope(std::nullopt, {0, 0});

        /*
         * fn f() -> str
         * {
         *     return "\tTest\n";
         * }
         */

        auto* fn_f = codegen_ctx.create_function(
          "f",
          cg::type{mock_str_type, cg::type_kind::str},
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

        co_ctx.push_scope(
          std::nullopt,
          {0, 0});

        const const_::constant_id string_id = const_env.intern("\tTest\n");

        codegen_ctx.generate_const(
          {mock_str_type, cg::type_kind::str},
          string_id);
        codegen_ctx.generate_ret(
          std::make_optional<cg::type>(
            mock_str_type, cg::type_kind::str));

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
