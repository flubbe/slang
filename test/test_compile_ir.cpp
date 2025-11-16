/**
 * slang - a simple scripting language.
 *
 * Compilation to IR tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
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

class test_name_resolver : public cg::name_resolver
{
    /** The semantic environment. */
    sema::env& sema_env;

    /** The constant environment. */
    const_::env& const_env;

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
     * @param const_env The constant environment.
     * @param type_ctx The type context.
     */
    test_name_resolver(
      sema::env& sema_env,
      const_::env& const_env,
      ty::context& type_ctx)
    : sema_env{sema_env}
    , const_env{const_env}
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

        if(it->second.declaring_module == sema::symbol_info::current_module_id)
        {
            return it->second.name;
        }

        return it->second.qualified_name;
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
        auto it = const_env.const_literal_map.find(id);
        if(it == const_env.const_literal_map.end())
        {
            throw std::runtime_error(
              std::format(
                "Constant with id {} not found in constant literal table.",
                id));
        }

        switch(it->second.type)
        {
        case const_::constant_type::i32:
            return std::format(
              "{}",
              static_cast<std::int32_t>(
                std::get<std::int64_t>(it->second.value)));
        case const_::constant_type::i64:
            return std::format(
              "{}",
              std::get<std::int64_t>(it->second.value));
        case const_::constant_type::f32:
            return std::format(
              "{}",
              static_cast<float>(
                std::get<double>(it->second.value)));
        case const_::constant_type::f64:
            return std::format(
              "{}",
              std::get<double>(it->second.value));
        case const_::constant_type::str:
            return std::format("{}", std::get<std::string>(it->second.value));
        default:;
            // fall-through.
        }

        throw std::runtime_error(
          std::format(
            "Constant with id {} has unknown type '{}'.",
            id,
            std::to_underlying(it->second.type)));
    }
};

cg::context get_context(
  sema::env& sema_env,
  const_::env& const_env,
  tl::context& lowering_ctx)
{
    cg::context ctx{sema_env, const_env, lowering_ctx};
    ctx.clear_flag(cg::codegen_flags::enable_const_eval);
    return ctx;
}

TEST(compile_ir, empty)
{
    // test: empty input
    const std::string test_input;

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
    cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

    test_name_resolver resolver{sema_env, const_env, type_ctx};

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast->collect_attributes(sema_env));
    ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->define_types(type_ctx));
    ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->generate_code(ctx));

    EXPECT_EQ(ctx.to_string(&resolver).length(), 0);
}

TEST(compile_ir, double_definition)
{
    {
        // test: global variable redefinition
        const std::string test_input =
          "let a: i32;\n"
          "let a: f32;";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);
        EXPECT_THROW(ast->collect_names(co_ctx), co::redefinition_error);
    }
    {
        // test: local variable redefinition
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i32;\n"
          " let a: f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);
        EXPECT_THROW(ast->collect_names(co_ctx), co::redefinition_error);
    }
    {
        // test: function redefinition
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          "}\n"
          "fn f() -> void\n"
          "{\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);
        co::context co_ctx{sema_env};
        EXPECT_THROW(ast->collect_names(co_ctx), co::redefinition_error);
    }
}

TEST(compile_ir, builtin_types)
{
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8;\n"
          " let b: i16;\n"
          " let c: i32;\n"
          " let d: i64;\n"
          " let e: f32;\n"
          " let f: f64;\n"
          " let s: str;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local i16 %2\n"
                  "local i32 %3\n"
                  "local i64 %4\n"
                  "local f32 %5\n"
                  "local f64 %6\n"
                  "local str %7\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_i32)
{
    // conversions: i32 -> i8, i16, i64, f32, f64
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_i8)
{
    // conversions: i8 -> i16, i32, i64, f32, f64
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          " let b: i16 = a as i16;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local i16 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " load i8 %1\n"    // sign-extended to i32
                  " cast i32_to_i16\n"
                  " store i16 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          " let b: i32 = a as i32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " load i8 %1\n"    // sign-extended to i32
                  " store i32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          " let b: i64 = a as i64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local i64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " load i8 %1\n"    // sign-extended to i32
                  " cast i32_to_i64\n"
                  " store i64 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          " let b: f32 = a as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local f32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " load i8 %1\n"    // sign-extended to i32
                  " cast i32_to_f32\n"
                  " store f32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i8 = 2 as i8;\n"
          " let b: f64 = a as f64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i8 %1\n"
                  "local f64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i8\n"
                  " store i8 %1\n"
                  " load i8 %1\n"    // sign-extended to i32
                  " cast i32_to_f64\n"
                  " store f64 %2\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_i16)
{
    // conversions: i16 -> i8, i32, i64, f32, f64
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          " let b: i8 = a as i8;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "local i8 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " load i16 %1\n"    // sign-extended to i32
                  " cast i32_to_i8\n"
                  " store i8 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          " let b: i32 = a as i32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " load i16 %1\n"    // sign-extended to i32
                  " store i32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          " let b: i64 = a as i64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "local i64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " load i16 %1\n"    // sign-extended to i32
                  " cast i32_to_i64\n"
                  " store i64 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          " let b: f32 = a as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "local f32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " load i16 %1\n"    // sign-extended to i32
                  " cast i32_to_f32\n"
                  " store f32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i16 = 2 as i16;\n"
          " let b: f64 = a as f64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i16 %1\n"
                  "local f64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i16\n"
                  " store i16 %1\n"
                  " load i16 %1\n"    // sign-extended to i32
                  " cast i32_to_f64\n"
                  " store f64 %2\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_i64)
{
    // conversions: i64 -> i8, i16, i32, f32, f64
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          " let b: i8 = a as i8;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "local i8 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " load i64 %1\n"
                  " cast i64_to_i32\n"
                  " cast i32_to_i8\n"
                  " store i8 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          " let b: i16 = a as i16;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "local i16 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " load i64 %1\n"
                  " cast i64_to_i32\n"
                  " cast i32_to_i16\n"
                  " store i16 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          " let b: i32 = a as i32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " load i64 %1\n"
                  " cast i64_to_i32\n"
                  " store i32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          " let b: f32 = a as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "local f32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " load i64 %1\n"
                  " cast i64_to_f32\n"
                  " store f32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i64 = 2 as i64;\n"
          " let b: f64 = a as f64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i64 %1\n"
                  "local f64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_i64\n"
                  " store i64 %1\n"
                  " load i64 %1\n"
                  " cast i64_to_f64\n"
                  " store f64 %2\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_f32)
{
    // conversions: f32 -> i8, i16, i32, i64, f64
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          " let b: i8 = a as i8;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "local i8 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " load f32 %1\n"
                  " cast f32_to_i32\n"
                  " cast i32_to_i8\n"
                  " store i8 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          " let b: i16 = a as i16;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "local i16 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " load f32 %1\n"
                  " cast f32_to_i32\n"
                  " cast i32_to_i16\n"
                  " store i16 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          " let b: i32 = a as i32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " load f32 %1\n"
                  " cast f32_to_i32\n"
                  " store i32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          " let b: i64 = a as i64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "local i64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " load f32 %1\n"
                  " cast f32_to_i64\n"
                  " store i64 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f32 = 2 as f32;\n"
          " let b: f64 = a as f64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f32 %1\n"
                  "local f64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f32\n"
                  " store f32 %1\n"
                  " load f32 %1\n"
                  " cast f32_to_f64\n"
                  " store f64 %2\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, convert_f64)
{
    // conversions: f64 -> i8, i16, i32, i64, f32
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          " let b: i8 = a as i8;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "local i8 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " load f64 %1\n"
                  " cast f64_to_i32\n"
                  " cast i32_to_i8\n"
                  " store i8 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          " let b: i16 = a as i16;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "local i16 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " load f64 %1\n"
                  " cast f64_to_i32\n"
                  " cast i32_to_i16\n"
                  " store i16 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          " let b: i32 = a as i32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " load f64 %1\n"
                  " cast f64_to_i32\n"
                  " store i32 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          " let b: i64 = a as i64;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "local i64 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " load f64 %1\n"
                  " cast f64_to_i64\n"
                  " store i64 %2\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: f64 = 2 as f64;\n"
          " let b: f32 = a as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local f64 %1\n"
                  "local f32 %2\n"
                  "entry:\n"
                  " const i32 2\n"
                  " cast i32_to_f64\n"
                  " store f64 %1\n"
                  " load f64 %1\n"
                  " cast f64_to_f32\n"
                  " store f32 %2\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, arithmetic_i8)
{
    {
        {
            const std::string test_input =
              "fn f() -> void\n"
              "{\n"
              " let a: i32 = 2;\n"
              " let b: i8 = (3*a) as i8;\n"
              "}";

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
            cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

            ASSERT_NO_THROW(ast->collect_names(co_ctx));
            ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
            ASSERT_NO_THROW(ast->collect_attributes(sema_env));
            ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->define_types(type_ctx));
            ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->generate_code(ctx));

            EXPECT_EQ(ctx.to_string(),
                      "define void @f() {\n"
                      "local i32 %1\n"
                      "local i8 %2\n"
                      "entry:\n"
                      " const i32 2\n"
                      " store i32 %1\n"
                      " const i32 3\n"
                      " load i32 %1\n"
                      " mul i32\n"
                      " cast i32_to_i8\n"
                      " store i8 %2\n"
                      " ret void\n"
                      "}");
        }
    }
}

TEST(compile_ir, arithmetic_i16)
{
    {
        {
            const std::string test_input =
              "fn f() -> void\n"
              "{\n"
              " let a: i32 = 2;\n"
              " let b: i16 = (3*a) as i16;\n"
              "}";

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
            cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

            ASSERT_NO_THROW(ast->collect_names(co_ctx));
            ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
            ASSERT_NO_THROW(ast->collect_attributes(sema_env));
            ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->define_types(type_ctx));
            ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
            ASSERT_NO_THROW(ast->generate_code(ctx));

            EXPECT_EQ(ctx.to_string(),
                      "define void @f() {\n"
                      "local i32 %1\n"
                      "local i16 %2\n"
                      "entry:\n"
                      " const i32 2\n"
                      " store i32 %1\n"
                      " const i32 3\n"
                      " load i32 %1\n"
                      " mul i32\n"
                      " cast i32_to_i16\n"
                      " store i16 %2\n"
                      " ret void\n"
                      "}");
        }
    }
}

TEST(compile_ir, logical_operators)
{
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i32 = 1 && 2;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 0\n"
                  " cmpne i32\n"
                  " jnz %0, %1\n"
                  "0:\n"
                  " const i32 2\n"
                  " const i32 0\n"
                  " cmpne i32\n"
                  " jmp %2\n"
                  "1:\n"
                  " const i32 0\n"
                  " jmp %2\n"
                  "2:\n"
                  " store i32 %1\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let a: i32 = 1 || 2;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 0\n"
                  " cmpeq i32\n"
                  " jnz %0, %1\n"
                  "0:\n"
                  " const i32 2\n"
                  " const i32 0\n"
                  " cmpne i32\n"
                  " jmp %2\n"
                  "1:\n"
                  " const i32 1\n"
                  " jmp %2\n"
                  "2:\n"
                  " store i32 %1\n"
                  " ret void\n"
                  "}");
    }
}

TEST(compile_ir, empty_function)
{
    // test: code generation for empty function
    const std::string test_input =
      "fn f() -> void\n"
      "{\n"
      "}";

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
    cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast->collect_attributes(sema_env));
    ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->define_types(type_ctx));
    ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->generate_code(ctx));

    EXPECT_EQ(ctx.to_string(),
              "define void @f() {\n"
              "entry:\n"
              " ret void\n"
              "}");
}

TEST(compile_ir, builtin_return_values)
{
    {
        // test: return i32 from function
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " return 1;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(&resolver),
                  "define i32 @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: return f32 from functino
        const std::string test_input =
          "fn f() -> f32\n"
          "{\n"
          " return 1.323 as f32;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(&resolver),
                  "define f32 @f() {\n"
                  "entry:\n"
                  " const f64 1.323\n"
                  " cast f64_to_f32\n"
                  " ret f32\n"
                  "}");
    }
    {
        // test: return str from function
        const std::string test_input =
          "fn f() -> str\n"
          "{\n"
          " return \"test\";\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(&resolver),
                  ".string @0 \"test\"\n"
                  "define str @f() {\n"
                  "entry:\n"
                  " const str @0\n"
                  " ret str\n"
                  "}");
    }
}

TEST(compile_ir, function_arguments_and_locals)
{
    {
        // test: empty function with arguments
        const std::string test_input =
          "fn f(i: i32, j: str, k: f32) -> void\n"
          "{\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %1, str %2, f32 %3) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        // test: function with arguments, locals and return statement
        const std::string test_input =
          "fn f(i: i32, j: str, k: f32) -> i32\n"
          "{\n"
          " let a: i32 = 1;\n"
          " return a;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %1, str %2, f32 %3) {\n"
                  "local i32 %4\n"
                  "entry:\n"
                  " const i32 1\n"
                  " store i32 %4\n"
                  " load i32 %4\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: assign locals from argument and return local's value
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " let a: i32 = j;\n"
          " return a;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %1, i32 %2, f32 %3) {\n"
                  "local i32 %4\n"
                  "entry:\n"
                  " load i32 %2\n"
                  " store i32 %4\n"
                  " load i32 %4\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: assign argument and return another argument's value.
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " i = 3;\n"
          " return j;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %1, i32 %2, f32 %3) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " store i32 %1\n"
                  " load i32 %2\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: chained assignments of arguments
        const std::string test_input =
          "fn f(i: i32, j: i32, k: f32) -> i32\n"
          "{\n"
          " i = j = 3;\n"
          " return j;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(&resolver),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " dup cat1\n"
                  " store i32 %j\n"
                  " store i32 %i\n"
                  " load i32 %j\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, arrays)
{
    {
        // test: array definition
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(&resolver),
                  "define void @f() {\n"
                  "local ref %b\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " dup ref\n"        // array_ref
                  " const i32 0\n"    // index
                  " const i32 1\n"    // value
                  " store_element i32\n"
                  " dup ref\n"        // array_ref
                  " const i32 1\n"    // index
                  " const i32 2\n"    // value
                  " store_element i32\n"
                  " store ref %b\n"
                  " ret void\n"
                  "}");
    }
    {
        // test: array definition and read access
        const std::string test_input =
          "fn f() -> [i32]\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          " return b;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define ref @f() {\n"
                  "local ref %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " dup ref\n"        // array_ref
                  " const i32 0\n"    // index
                  " const i32 1\n"    // value
                  " store_element i32\n"
                  " dup ref\n"        // array_ref
                  " const i32 1\n"    // index
                  " const i32 2\n"    // value
                  " store_element i32\n"
                  " store ref %1\n"
                  " load ref %1\n"
                  " ret ref<type#9>\n"
                  "}");
    }
    {
        // test: array definition via new, read and write
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " b = new i32[2];\n"
          " b[1] = 2;\n"
          " return b[0];\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local ref %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " store ref %1\n"
                  " load ref %1\n"    // array_ref
                  " const i32 1\n"    // index
                  " const i32 2\n"    // value
                  " store_element i32\n"
                  " load ref %1\n"
                  " const i32 0\n"
                  " load_element i32\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: chained element assignment
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " b = new i32[2];\n"
          " b[0] = b[1] = 2;\n"
          " return b[0];\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local ref %1\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " store ref %1\n"
                  " load ref %1\n"               // array_ref
                  " const i32 0\n"               // index
                  " load ref %1\n"               // array_ref
                  " const i32 1\n"               // index
                  " const i32 2\n"               // value
                  " dup_x2 cat1, cat1, ref\n"    // duplicate i32 value and store it (i32, ref) down the stack
                  " store_element i32\n"
                  " store_element i32\n"
                  " load ref %1\n"
                  " const i32 0\n"
                  " load_element i32\n"
                  " ret i32\n"
                  "}");
    }
    {
        // test: invalid new operator syntax
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " b = new i32;\n"    // missing size
          " b[1] = 2;\n"
          " return b[0];\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test: invalid new operator syntax
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " b = new i32[];\n"    // missing size
          " b[1] = 2;\n"
          " return b[0];\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        // test: invalid new operator syntax
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " b = new [i32];\n"    // invalid type
          " b[1] = 2;\n"
          " return b[0];\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        EXPECT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(compile_ir, unary_operators)
{
    {
        const std::string test_input =
          "fn local(a: i32) -> i32 {\n"
          " let b: i32 = -1;\n"
          " return a+b;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @local(i32 %1) {\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 0\n"
                  " const i32 1\n"
                  " sub i32\n"
                  " store i32 %2\n"
                  " load i32 %1\n"
                  " load i32 %2\n"
                  " add i32\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn local(a: i32) -> i32 {\n"
          " let b: i32 = ~1;\n"
          " return a+b;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @local(i32 %1) {\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 -1\n"
                  " const i32 1\n"
                  " xor i32\n"
                  " store i32 %2\n"
                  " load i32 %1\n"
                  " load i32 %2\n"
                  " add i32\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, binary_operators)
{
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 1*2 + 3;\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " mul i32\n"
                  " const i32 3\n"
                  " add i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 1*(2+3);\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " add i32\n"
                  " mul i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 6/(2-3);\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 6\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " sub i32\n"
                  " div i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 1 & 2 | 4 << 2 >> 1;\n"    // same as (1 & 2) | ((4 << 2) >> 1).
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " and i32\n"
                  " const i32 4\n"
                  " const i32 2\n"
                  " shl i32\n"
                  " const i32 1\n"
                  " shr i32\n"
                  " or i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 1 > 2 | 3 < 4 & 4;\n"    // same as (1 > 2) | ((3 < 4) & 4)
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " cmpg i32\n"
                  " const i32 3\n"
                  " const i32 4\n"
                  " cmpl i32\n"
                  " const i32 4\n"
                  " and i32\n"
                  " or i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 5 <= 7 ^ 2 & 2 >= 1;\n"    // same as (5 <= 7) ^ (2 & (2 >= 1))
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 5\n"
                  " const i32 7\n"
                  " cmple i32\n"
                  " const i32 2\n"
                  " const i32 2\n"
                  " const i32 1\n"
                  " cmpge i32\n"
                  " and i32\n"
                  " xor i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, postfix_operators)
{
    const std::string test_input =
      "fn f() -> void\n"
      "{\n"
      " let i: i32 = 0;\n"
      " i++;\n"
      "}";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    std::shared_ptr<ast::expression> ast = parser.get_ast();
    sema::env sema_env;
    const_::env const_env;
    macro::env macro_env;
    co::context co_ctx{sema_env};
    ty::context type_ctx;
    rs::context resolver_ctx{sema_env, const_env, macro_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
    ASSERT_NO_THROW(ast->collect_attributes(sema_env));
    ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->define_types(type_ctx));
    ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
    ASSERT_NO_THROW(ast->generate_code(ctx));

    EXPECT_EQ(ctx.to_string(),
              "define void @f() {\n"
              "local i32 %1\n"
              "entry:\n"
              " const i32 0\n"
              " store i32 %1\n"
              " load i32 %1\n"
              " dup cat1\n"
              " const i32 1\n"
              " add i32\n"
              " store i32 %1\n"
              " pop i32\n"
              " ret void\n"
              "}");
}

TEST(compile_ir, compound_assignments)
{
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 0;\n"
          " i += 1;\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " const i32 1\n"
                  " add i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 0;\n"
          " let j: i32 = 1;\n"
          " i += j += 1;\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %1\n"
                  " const i32 1\n"
                  " store i32 %2\n"
                  " load i32 %1\n"
                  " load i32 %2\n"
                  " const i32 1\n"
                  " add i32\n"
                  " dup cat1\n"
                  " store i32 %2\n"
                  " add i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 0;\n"
          " let j: i32 = 1;\n"
          " i += j + 2;\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %1\n"
                  "local i32 %2\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %1\n"
                  " const i32 1\n"
                  " store i32 %2\n"
                  " load i32 %1\n"
                  " load i32 %2\n"
                  " const i32 2\n"
                  " add i32\n"
                  " add i32\n"
                  " store i32 %1\n"
                  " load i32 %1\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let i: i32 = 0;\n"
          " let j: i32 = 1;\n"
          " i += j + 2 += 1;\n"
          " return i;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        EXPECT_THROW(ast->generate_code(ctx), cg::codegen_error);
    }
}

TEST(compile_ir, function_calls)
{
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> void\n"
          "{}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke <func#1>\n"
                  " ret void\n"
                  "}\n"
                  "define void @g() {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g(1, 2.3 as f32, \"Test\", h());\n"
          "}\n"
          "fn g(a: i32, b: f32, c: str, d: i32) -> void\n"
          "{}\n"
          "fn h() -> i32 {\n"
          " return 0;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  ".string @0 \"Test\"\n"
                  "define void @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const f64 2.3\n"
                  " cast f64_to_f32\n"
                  " const str @0\n"
                  " invoke <func#6>\n"
                  " invoke <func#1>\n"
                  " ret void\n"
                  "}\n"
                  "define void @g(i32 %2, f32 %3, str %4, i32 %5) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}\n"
                  "define i32 @h() {\n"
                  "entry:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g(1 + 2 * 3, 2.3 as f32);\n"
          "}\n"
          "fn g(i: i32, j:f32) -> void {\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " mul i32\n"
                  " add i32\n"
                  " const f64 2.3\n"
                  " cast f64_to_f32\n"
                  " invoke <func#1>\n"
                  " ret void\n"
                  "}\n"
                  "define void @g(i32 %2, f32 %3) {\n"
                  "entry:\n"
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "fn arg(a: i32) -> i32 {\n"
          " return 1 + a;\n"
          "}\n"
          "fn arg2(a: i32) -> i32 {\n"
          " return arg(a) - 1;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));
    }
    {
        // popping of unused return values (i32 and f32)
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          " h();\n"
          "}\n"
          "fn g() -> i32\n"
          "{\n"
          " return 0;\n"
          "}\n"
          "fn h() -> f32\n"
          "{\n"
          " return -1.0 as f32;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke <func#1>\n"
                  " pop i32\n"
                  " invoke <func#2>\n"
                  " pop f32\n"
                  " ret void\n"
                  "}\n"
                  "define i32 @g() {\n"
                  "entry:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}\n"
                  "define f32 @h() {\n"
                  "entry:\n"
                  " const f32 0\n"
                  " const f64 1\n"
                  " cast f64_to_f32\n"
                  " sub f32\n"
                  " ret f32\n"
                  "}");
    }
    {
        // popping of unused return values (struct)
        const std::string test_input =
          "struct S {\n"
          " i: i32\n"
          "};\n"
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> S\n"
          "{\n"
          " return S{i: 1};\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  "}\n"
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke <func#3>\n"
                  " pop ref<type#9>\n"
                  " ret void\n"
                  "}\n"
                  "define ref @g() {\n"
                  "entry:\n"
                  " new ref<type#9>\n"
                  " dup ref\n"
                  " const i32 1\n"
                  " set_field <type#9>.<field#0>\n"
                  " ret ref<type#9>\n"
                  "}");
    }
}

TEST(compile_ir, if_statement)
{
    {
        const std::string test_input =
          "fn test_if_else(a: i32) -> i32\n"
          "{\n"
          " if(a > 0)\n"
          " {\n"
          "  return 1;\n"
          " }\n"
          " else\n"
          " {\n"
          "  return 0;\n"
          " }\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test_if_else(i32 %1) {\n"
                  "entry:\n"
                  " load i32 %1\n"
                  " const i32 0\n"
                  " cmpg i32\n"
                  " jnz %0, %2\n"
                  "0:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  "2:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, break_fail)
{
    {
        const std::string test_input =
          "fn test_break_fail(a: i32) -> i32\n"
          "{\n"
          " break;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        EXPECT_THROW(ast->generate_code(ctx), cg::codegen_error);
    }
}

TEST(compile_ir, continue_fail)
{
    {
        const std::string test_input =
          "fn test_continue_fail(a: i32) -> i32\n"
          "{\n"
          " continue;\n"
          "}";

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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        EXPECT_THROW(ast->generate_code(ctx), cg::codegen_error);
    }
}

TEST(compile_ir, structs)
{
    {
        // named initialization.
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " j: f32\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = S{ i: 2, j: 3 as f32 };\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        test_name_resolver resolver{sema_env, const_env, type_ctx};

        EXPECT_EQ(ctx.to_string(&resolver),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local ref %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup ref\n"
                  " const i32 2\n"
                  " set_field %S.i\n"
                  " dup ref\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field %S.j\n"
                  " store ref %s\n"
                  " ret void\n"
                  "}");
    }
    {
        // re-ordered named initialization.
        const std::string test_input =
          "struct S {\n"
          " j: f32,\n"
          " i: i32\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = S{ i: 2, j: 3 as f32 };\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " f32 %j,\n"
                  " i32 %i,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"
                  " dup ref\n"
                  " const i32 2\n"
                  " set_field <type#9>.<field#1>\n"
                  " dup ref\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field <type#9>.<field#0>\n"
                  " store ref %4\n"
                  " ret void\n"
                  "}");
    }
    {
        // anonymous initialization.
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " j: f32\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = S{ 2, 3 as f32 };\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"
                  " dup ref\n"
                  " const i32 2\n"
                  " set_field <type#9>.<field#0>\n"
                  " dup ref\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field <type#9>.<field#1>\n"
                  " store ref %4\n"
                  " ret void\n"
                  "}");
    }
    {
        // member access and conversions.
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " j: f32\n"
          "};\n"
          "fn test() -> i32\n"
          "{\n"
          " let s: S = S{ i: 2, j: 3 as f32 };\n"
          " s.i = 1;\n"
          " return s.i + s.j as i32;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"
                  " dup ref\n"
                  " const i32 2\n"
                  " set_field <type#9>.<field#0>\n"
                  " dup ref\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field <type#9>.<field#1>\n"
                  " store ref %4\n"
                  " load ref %4\n"
                  " const i32 1\n"
                  " set_field <type#9>.<field#0>\n"
                  " load ref %4\n"
                  " get_field <type#9>.<field#0>\n"
                  " load ref %4\n"
                  " get_field <type#9>.<field#1>\n"
                  " cast f32_to_i32\n"
                  " add i32\n"
                  " ret i32\n"
                  "}");
    }
    {
        // member access in chained assignments.
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " j: i32\n"
          "};\n"
          "fn test() -> i32\n"
          "{\n"
          " let s: S = S{ i: 2, j: 3 };\n"
          " s.i = s.j = 1;\n"
          " return s.i + s.j;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " i32 %j,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"
                  " dup ref\n"
                  " const i32 2\n"
                  " set_field <type#9>.<field#0>\n"
                  " dup ref\n"
                  " const i32 3\n"
                  " set_field <type#9>.<field#1>\n"
                  " store ref %4\n"
                  " load ref %4\n"                     // [addr]
                  " load ref %4\n"                     // [addr, addr]
                  " const i32 1\n"                     // [addr, addr, 1]
                  " dup_x1 cat1, ref\n"                // [addr, 1, addr, 1]
                  " set_field <type#9>.<field#1>\n"    // [addr, 1]
                  " set_field <type#9>.<field#0>\n"    // []
                  " load ref %4\n"                     // [addr]
                  " get_field <type#9>.<field#0>\n"    // [1]
                  " load ref %4\n"                     // [1, addr]
                  " get_field <type#9>.<field#1>\n"    // [1, 1]
                  " add i32\n"                         // [2]
                  " ret i32\n"
                  "}");
    }
}

TEST(compile_ir, nested_structs)
{
    {
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " next: S\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = S{ i: 1, next: S{i: 3, next: null} };\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"                 // [addr1]
                  " dup ref\n"                         // [addr1, addr1]
                  " const i32 1\n"                     // [addr1, addr1, 1]
                  " set_field <type#9>.<field#0>\n"    // [addr1]                              addr1.i = 1
                  " dup ref\n"                         // [addr1, addr1]
                  " new ref<type#9>\n"                 // [addr1, addr1, addr2]
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"                     // [addr1, addr1, addr2, addr2, 3]
                  " set_field <type#9>.<field#0>\n"    // [addr1, addr1, addr2]                addr2.i = 3
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const_null\n"                      // [addr1, addr1, addr2, addr2, null]
                  " set_field <type#9>.<field#1>\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field <type#9>.<field#1>\n"    // [addr1]                              addr1.next = addr2
                  " store ref %4\n"                    // []                                   s = addr1
                  " ret void\n"
                  "}");
    }
    {
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " next: S\n"
          "};\n"
          "fn test() -> i32\n"
          "{\n"
          " let s: S = S{ i: 1, next: S{i: 3, next: null} };\n"
          " return s.next.i;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"                 // [addr1]
                  " dup ref\n"                         // [addr1, addr1]
                  " const i32 1\n"                     // [addr1, addr1, 1]
                  " set_field <type#9>.<field#0>\n"    // [addr1]                              addr1.i = 1
                  " dup ref\n"                         // [addr1, addr1]
                  " new ref<type#9>\n"                 // [addr1, addr1, addr2]
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"                     // [addr1, addr1, addr2, addr2, 3]
                  " set_field <type#9>.<field#0>\n"    // [addr1, addr1, addr2]                addr2.i = 3
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const_null\n"                      // [addr1, addr1, addr2, addr2, null]
                  " set_field <type#9>.<field#1>\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field <type#9>.<field#1>\n"    // [addr1]                              addr1.next = addr2
                  " store ref %4\n"                    // []                                   s = addr1
                  " load ref %4\n"                     // [s]
                  " get_field <type#9>.<field#1>\n"    // [s.next]
                  " get_field <type#9>.<field#0>\n"    // [i]
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " next: S\n"
          "};\n"
          "fn test() -> i32\n"
          "{\n"
          " let s: S = S{ i: 1, next: S{i: 3, next: null} };\n"
          " s.next.next = s;\n"
          " s.next.next.i = 2;\n"
          " return s.i + s.next.i;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local ref %4\n"
                  "entry:\n"
                  " new ref<type#9>\n"                 // [addr1]
                  " dup ref\n"                         // [addr1, addr1]
                  " const i32 1\n"                     // [addr1, addr1, 1]
                  " set_field <type#9>.<field#0>\n"    // [addr1]                              addr1.i = 1
                  " dup ref\n"                         // [addr1, addr1]
                  " new ref<type#9>\n"                 // [addr1, addr1, addr2]
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"                     // [addr1, addr1, addr2, addr2, 3]
                  " set_field <type#9>.<field#0>\n"    // [addr1, addr1, addr2]                addr2.i = 3
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const_null\n"                      // [addr1, addr1, addr2, addr2, null]
                  " set_field <type#9>.<field#1>\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field <type#9>.<field#1>\n"    // [addr1]                              addr1.next = addr2
                  " store ref %4\n"                    // []                                   s = addr1
                  " load ref %4\n"                     // [s]
                  " get_field <type#9>.<field#1>\n"    // [s.next]
                  " load ref %4\n"                     // [s.next, s]
                  " set_field <type#9>.<field#1>\n"    // []                                   s.next.next = s
                  " load ref %4\n"                     // [s]
                  " get_field <type#9>.<field#1>\n"    // [s.next]
                  " get_field <type#9>.<field#1>\n"    // [s.next.next]
                  " const i32 2\n"                     // [s.next.next, 2]
                  " set_field <type#9>.<field#0>\n"    // []                                   s.next.next.i = 2
                  " load ref %4\n"                     // [s]
                  " get_field <type#9>.<field#0>\n"    // [i]
                  " load ref %4\n"                     // [i, s]
                  " get_field <type#9>.<field#1>\n"    // [i, s.next]
                  " get_field <type#9>.<field#0>\n"    // [i, s.next.i]
                  " add i32\n"                         // [i + s.next.i]
                  " ret i32\n"
                  "}");
    }
    {
        const std::string test_input =
          "struct Link {\n"
          " next: Link\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let root: Link = Link{next: Link{next: null}};\n"
          " root.next.next = root;\n"
          " root.next.next = null;\n"
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
        cg::context ctx = get_context(sema_env, const_env, lowering_ctx);

        ASSERT_NO_THROW(ast->collect_names(co_ctx));
        ASSERT_NO_THROW(ast->resolve_names(resolver_ctx));
        ASSERT_NO_THROW(ast->collect_attributes(sema_env));
        ASSERT_NO_THROW(ast->declare_types(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->define_types(type_ctx));
        ASSERT_NO_THROW(ast->declare_functions(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->type_check(type_ctx, sema_env));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%Link = type {\n"
                  " Link %next,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local ref %3\n"
                  "entry:\n"
                  " new ref<type#9>\n"                 // [addr1]
                  " dup ref\n"                         // [addr1, addr1]
                  " new ref<type#9>\n"                 // [addr1, addr1, addr2]
                  " dup ref\n"                         // [addr1, addr1, addr2, addr2]
                  " const_null\n"                      // [addr1, addr1, addr2, addr2, null]
                  " set_field <type#9>.<field#0>\n"    // [addr1, addr1, addr2]                   addr2.next = null
                  " set_field <type#9>.<field#0>\n"    // [addr1]                                 addr1.next = addr2
                  " store ref %3\n"                    // []                                      root = addr1
                  " load ref %3\n"                     // [root]
                  " get_field <type#9>.<field#0>\n"    // [root.next]
                  " load ref %3\n"                     // [root.next, root]
                  " set_field <type#9>.<field#0>\n"    // []                                      root.next.next = root
                  " load ref %3\n"                     // [root]
                  " get_field <type#9>.<field#0>\n"    // [root.next]
                  " const_null\n"                      // [root.next, null]
                  " set_field <type#9>.<field#0>\n"    // []                                      root.next.next = null
                  " ret void\n"
                  "}");
    }
}

}    // namespace
