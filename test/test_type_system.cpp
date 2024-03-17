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

#include "parser.h"
#include "codegen.h"
#include "typing.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;

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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        ast->collect_names(ctx);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        ast->collect_names(ctx);
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input = "let a: f32 = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input = "let a: str = \"test\";";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input = "let a: str = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
    }
    {
        const std::string test_input = "let a: i32 = 1.0;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
    }
    {
        const std::string test_input = "let a: f32 = \"Test\";";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2.) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2 as f32) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = ((1 + 1. as i32) as f32 * 2) as i32;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_NO_THROW(parser.parse(lexer));

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = 1. + 2.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: i32 = 1. + 2;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "let a: i32 = 1 >> 2;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
    {
        const std::string test_input =
          "let a: i32 = 1 >> 2.;";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_THROW(ast->type_check(ctx), ty::type_error);
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
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

        const slang::ast::block* ast = parser.get_ast();
        EXPECT_NE(ast, nullptr);

        ty::context ctx;
        EXPECT_NO_THROW(ast->collect_names(ctx));
        EXPECT_NO_THROW(ast->type_check(ctx));
    }
}

}    // namespace