/**
 * slang - a simple scripting language.
 *
 * Type system tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "compiler/codegen.h"
#include "compiler/parser.h"
#include "compiler/typing.h"
#include "resolve.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;

namespace
{

TEST(type_system, name_collection)
{
    {
        const std::string test_input =
          "fn f(i: f32) -> i32\n"
          "{\n"
          " return 1.;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, variables)
{
    {
        const std::string test_input = "let a: i32 = 1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input = "let a: f32 = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input = "let a: str = \"test\";";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input = "let a: str = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input = "let a: i32 = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input = "let a: f32 = \"Test\";";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: f32 = 1.;\n"
          "let b: f32 = a;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: f32 = 1.;\n"
          "let b: str = a;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let s: S;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(type_system, explicit_cast)
{
    {
        const std::string test_input =
          "let a: i32 = 1. as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2.) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2 as f32) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: f32 = 1. as i32 as f32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_THROW(parser.parse(lexer), slang::syntax_error);
    }
    {
        const std::string test_input =
          "let a: f32 = (1. as i32) as f32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));
    }
}

TEST(type_system, binary_operators)
{
    {
        const std::string test_input =
          "let a: f32 = 1. + 2.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = 1. + 2.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: i32 = 1. + 2;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: i32 = 1 >> 2;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = 1 >> 2.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(type_system, unary_operators)
{
    {
        const std::string test_input =
          "let a: i32 = +1;"
          "let b: i32 = -1;"
          "let c: i32 = ~1;"
          "let d: i32 = !1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = +(1 + 2);"
          "let b: i32 = -+1;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let a: f32 = ~1.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: i32 = !1.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: str = !\"test\";";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(type_system, functions)
{
    {
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

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(a: i32, b: f32) -> void\n"
          "{\n"
          " let c: i32 = 1. as i32;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(a: i32, b: f32) -> void\n"
          "{\n"
          " let b: i32 = 1. as i32;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn f(a: i32, b: S) -> void\n"
          "{\n"
          " let c: i32 = 1. as i32;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_THROW(type_ctx.resolve_types(), ty::type_error);
    }
    {
        const std::string test_input =
          "fn f(a: i32) -> void\n"
          "{\n"
          " let b: f32 = 2.;\n"
          " let b: i32 = 1. as i32;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(type_system, arrays)
{
    {
        const std::string test_input =
          "fn array_init() -> [i32]\n"
          "{\n"
          " let b: [i32] = [1, 2, 3];\n"
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> i32\n"
          "{\n"
          " let b: [i32] = [\"s\"];\n"    // wrong type
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> i32\n"
          "{\n"
          " let b: [i32] = [1, \"s\"];\n"    // inconsistent types
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> i32\n"
          "{\n"
          " let b: [i32] = new i32[2];\n"
          " return b;\n"    // does not match return type
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> [i32]\n"
          "{\n"
          " let b: [i32] = new T[2];\n"    // unknown type
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> [i32]\n"
          "{\n"
          " let b: [i32] = new i32[2.123];\n"    // invalid size type
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(type_system, structs)
{
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn test(a: S) -> void\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_THROW(type_ctx.resolve_types(), ty::type_error);
    }
    {
        const std::string test_input =
          "fn test() -> S\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_THROW(type_ctx.resolve_types(), ty::type_error);
    }
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};\n"
          "fn test(a: S) -> void\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn test(a: S) -> void\n"
          "{\n"
          "}\n"
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};\n"
          "let s: S;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "let s: S;\n"
          "struct S\n"
          "{\n"
          " a: i32,\n"
          " b: f32\n"
          "};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, function_calls)
{
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          " f();\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(i: i32) -> void\n"
          "{\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          " f(1);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(i: f32) -> void\n"
          "{\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          " f(1);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn f(i: f32) -> void\n"
          "{\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          " f(1 as f32);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(i: f32) -> i32\n"
          "{\n"
          " return 1;\n"
          "}\n"
          "fn g() -> void\n"
          "{\n"
          " let a: i32 = f(1 as f32);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, return_expressions)
{
    {
        const std::string test_input =
          "fn f(i: f32) -> i32\n"
          "{\n"
          " return 1.;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn f(i: f32) -> i32\n"
          "{\n"
          " return 1. as i32;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "fn f(i: i32) -> i32\n"
          "{\n"
          " return 1. as i32;\n"
          "}\n"
          "fn g(x: f32) -> f32\n"
          "{\n"
          " return f(x as i32) as f32;"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, element_access)
{
    {
        const std::string test_input =
          "struct S{a: i32};\n"
          "fn f() -> void\n"
          "{\n"
          " let s: S;\n"
          " s.a = 3;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "struct S{a: i32};\n"
          "fn f() -> void\n"
          "{\n"
          " let s: S;\n"
          " s.a = 3.2;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "struct S{a: i32, b: f32};\n"
          "struct T{s: S};\n"
          "fn f() -> void\n"
          "{\n"
          " let t: T;\n"
          " t.s.a = 3;\n"
          " t.s.b = 1.2;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "struct S{a: i32, b: str};\n"
          "struct T{s: S};\n"
          "fn f() -> void\n"
          "{\n"
          " let t: T;\n"
          " t.s.a = 3;\n"
          " t.s.b = 1.2;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "struct S{a: i32, b: i32};\n"
          "struct T{s: S};\n"
          "fn f() -> void\n"
          "{\n"
          " let t: T;\n"
          " t.s.a = 3;\n"
          " t.s.b = t.s.a;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "struct S{a: i32, b: str};\n"
          "struct T{s: S};\n"
          "fn f() -> void\n"
          "{\n"
          " let t: T;\n"
          " t.s.b = \"test\";\n"
          " t.s.a = t.s.b;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "struct S{a: i32};\n"
          "fn f(a: i32) -> void {};\n"
          "fn g() -> void\n"
          "{\n"
          " let s: S;\n"
          " s.a = -23;\n"
          " f(s.a);\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, examples)
{
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

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
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

        ty::context type_ctx;
        cg::context codegen_ctx;
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\tstd::println(\"Hello, World!\");\n"
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;
        mgr.add_search_path("lang");
        ASSERT_TRUE(mgr.is_file("std.cmod"));

        ty::context type_ctx;
        cg::context codegen_ctx;
        rs::context resolve_ctx{mgr};
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

TEST(type_system, native_binding)
{
    {
        const std::string test_input =
          "/**\n"
          " * Print a string to stdout.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=\"slang\")]\n"
          "fn print(s: str) -> void;\n"
          "\n"
          "/**\n"
          " * Print a string to stdout and append a new-line character.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=\"slang\")]\n"
          "fn println(s: str) -> void;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();

        slang::file_manager mgr;
        ty::context type_ctx;
        cg::context codegen_ctx;
        rs::context resolve_ctx{mgr};
        EXPECT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        EXPECT_NO_THROW(type_ctx.resolve_types());
        EXPECT_NO_THROW(ast->type_check(type_ctx));
    }
}

}    // namespace
