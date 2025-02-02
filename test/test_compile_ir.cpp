/**
 * slang - a simple scripting language.
 *
 * Compilation to IR tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <memory>

#include <fmt/core.h>
#include <gtest/gtest.h>

#include "compiler/parser.h"
#include "compiler/codegen.h"
#include "compiler/typing.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace ty = slang::typing;

namespace
{

static cg::context get_context()
{
    cg::context ctx;
    ctx.evaluate_constant_subexpressions = false;
    return ctx;
}

TEST(compile_ir, empty)
{
    // test: empty input
    const std::string test_input = "";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    std::shared_ptr<ast::expression> ast = parser.get_ast();
    cg::context ctx = get_context();
    ty::context type_ctx;
    ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
    ASSERT_NO_THROW(type_ctx.resolve_types());
    ASSERT_NO_THROW(ast->type_check(type_ctx));
    ASSERT_NO_THROW(ast->generate_code(ctx));

    EXPECT_EQ(ctx.to_string().length(), 0);
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        EXPECT_THROW(ast->collect_names(ctx, type_ctx), cg::codegen_error);
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
    cg::context ctx = get_context();
    ty::context type_ctx;
    ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
    ASSERT_NO_THROW(type_ctx.resolve_types());
    ASSERT_NO_THROW(ast->type_check(type_ctx));
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
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
          " return 1.323;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define f32 @f() {\n"
                  "entry:\n"
                  " const f32 1.323\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f(i32 %i, str %j, f32 %k) {\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, str %j, f32 %k) {\n"
                  "local i32 %a\n"
                  "entry:\n"
                  " const i32 1\n"
                  " store i32 %a\n"
                  " load i32 %a\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "local i32 %a\n"
                  "entry:\n"
                  " load i32 %j\n"
                  " store i32 %a\n"
                  " load i32 %a\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " store i32 %i\n"
                  " load i32 %j\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f(i32 %i, i32 %j, f32 %k) {\n"
                  "entry:\n"
                  " const i32 3\n"
                  " dup i32\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "local [i32] %b\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " dup [i32]\n"      // array_ref
                  " const i32 0\n"    // index
                  " const i32 1\n"    // value
                  " store_element i32\n"
                  " dup [i32]\n"      // array_ref
                  " const i32 1\n"    // index
                  " const i32 2\n"    // value
                  " store_element i32\n"
                  " store [i32] %b\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define [i32] @f() {\n"
                  "local [i32] %b\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " dup [i32]\n"      // array_ref
                  " const i32 0\n"    // index
                  " const i32 1\n"    // value
                  " store_element i32\n"
                  " dup [i32]\n"      // array_ref
                  " const i32 1\n"    // index
                  " const i32 2\n"    // value
                  " store_element i32\n"
                  " store [i32] %b\n"
                  " load [i32] %b\n"
                  " ret [i32]\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local [i32] %b\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " store [i32] %b\n"
                  " load [i32] %b\n"    // array_ref
                  " const i32 1\n"      // index
                  " const i32 2\n"      // value
                  " store_element i32\n"
                  " load [i32] %b\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local [i32] %b\n"
                  "entry:\n"
                  " const i32 2\n"
                  " newarray i32\n"
                  " store [i32] %b\n"
                  " load [i32] %b\n"          // array_ref
                  " const i32 0\n"            // index
                  " load [i32] %b\n"          // array_ref
                  " const i32 1\n"            // index
                  " const i32 2\n"            // value
                  " dup i32, i32, @addr\n"    // duplicate i32 value and store it (i32, @addr) down the stack
                  " store_element i32\n"
                  " store_element i32\n"
                  " load [i32] %b\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @local(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " const i32 0\n"
                  " const i32 1\n"
                  " sub i32\n"
                  " store i32 %b\n"
                  " load i32 %a\n"
                  " load i32 %b\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @local(i32 %a) {\n"
                  "local i32 %b\n"
                  "entry:\n"
                  " const i32 -1\n"
                  " const i32 1\n"
                  " xor i32\n"
                  " store i32 %b\n"
                  " load i32 %a\n"
                  " load i32 %b\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " mul i32\n"
                  " const i32 3\n"
                  " add i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " add i32\n"
                  " mul i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "entry:\n"
                  " const i32 6\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " sub i32\n"
                  " div i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
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
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
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
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
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
                  " store i32 %i\n"
                  " load i32 %i\n"
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
    cg::context ctx = get_context();
    ty::context type_ctx;
    ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
    ASSERT_NO_THROW(type_ctx.resolve_types());
    ASSERT_NO_THROW(ast->type_check(type_ctx));
    ASSERT_NO_THROW(ast->generate_code(ctx));

    EXPECT_EQ(ctx.to_string(),
              "define void @f() {\n"
              "local i32 %i\n"
              "entry:\n"
              " const i32 0\n"
              " store i32 %i\n"
              " load i32 %i\n"
              " dup i32\n"
              " const i32 1\n"
              " add i32\n"
              " store i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
                  " const i32 1\n"
                  " add i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "local i32 %j\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %i\n"
                  " const i32 1\n"
                  " store i32 %j\n"
                  " load i32 %i\n"
                  " load i32 %j\n"
                  " const i32 1\n"
                  " add i32\n"
                  " dup i32\n"
                  " store i32 %j\n"
                  " add i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @f() {\n"
                  "local i32 %i\n"
                  "local i32 %j\n"
                  "entry:\n"
                  " const i32 0\n"
                  " store i32 %i\n"
                  " const i32 1\n"
                  " store i32 %j\n"
                  " load i32 %i\n"
                  " load i32 %j\n"
                  " const i32 2\n"
                  " add i32\n"
                  " add i32\n"
                  " store i32 %i\n"
                  " load i32 %i\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke @g\n"
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
          " g(1, 2.3, \"Test\", h());\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  ".string @0 \"Test\"\n"
                  "define void @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const f32 2.3\n"
                  " const str @0\n"
                  " invoke @h\n"
                  " invoke @g\n"
                  " ret void\n"
                  "}\n"
                  "define void @g(i32 %a, f32 %b, str %c, i32 %d) {\n"
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
          " g(1 + 2 * 3, 2.3);\n"
          "}\n"
          "fn g(i: i32, j:f32) -> void {\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " const i32 1\n"
                  " const i32 2\n"
                  " const i32 3\n"
                  " mul i32\n"
                  " add i32\n"
                  " const f32 2.3\n"
                  " invoke @g\n"
                  " ret void\n"
                  "}\n"
                  "define void @g(i32 %i, f32 %j) {\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
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
          " return -1.0;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::expression> ast = parser.get_ast();
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke @g\n"
                  " pop i32\n"
                  " invoke @h\n"
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
                  " const f32 1\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  "}\n"
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke @g\n"
                  " pop S\n"
                  " ret void\n"
                  "}\n"
                  "define S @g() {\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 1\n"
                  " set_field %S, i32 %i\n"
                  " ret S\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "define i32 @test_if_else(i32 %a) {\n"
                  "entry:\n"
                  " load i32 %a\n"
                  " const i32 0\n"
                  " cmpg i32\n"
                  " jnz %0, %2\n"
                  "0:\n"
                  " const i32 1\n"
                  " ret i32\n"
                  " jmp %1\n"
                  "2:\n"
                  " const i32 0\n"
                  " ret i32\n"
                  " jmp %1\n"
                  "1:\n"
                  " unreachable\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 2\n"
                  " set_field %S, i32 %i\n"
                  " dup S\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field %S, f32 %j\n"
                  " store S %s\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " f32 %j,\n"
                  " i32 %i,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 2\n"
                  " set_field %S, i32 %i\n"
                  " dup S\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field %S, f32 %j\n"
                  " store S %s\n"
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
        cg::context ctx = get_context();
        ty::context type_ctx;
        ASSERT_NO_THROW(ast->collect_names(ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(ctx));

        EXPECT_EQ(ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 2\n"
                  " set_field %S, i32 %i\n"
                  " dup S\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field %S, f32 %j\n"
                  " store S %s\n"
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " f32 %j,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 2\n"
                  " set_field %S, i32 %i\n"
                  " dup S\n"
                  " const i32 3\n"
                  " cast i32_to_f32\n"
                  " set_field %S, f32 %j\n"
                  " store S %s\n"
                  " load S %s\n"
                  " const i32 1\n"
                  " set_field %S, i32 %i\n"
                  " load S %s\n"
                  " get_field %S, i32 %i\n"
                  " load S %s\n"
                  " get_field %S, f32 %j\n"
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " i32 %j,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"
                  " dup S\n"
                  " const i32 2\n"
                  " set_field %S, i32 %i\n"
                  " dup S\n"
                  " const i32 3\n"
                  " set_field %S, i32 %j\n"
                  " store S %s\n"
                  " load S %s\n"               // [addr]
                  " load S %s\n"               // [addr, addr]
                  " const i32 1\n"             // [addr, addr, 1]
                  " dup i32, @addr\n"          // [addr, 1, addr, 1]
                  " set_field %S, i32 %j\n"    // [addr, 1]
                  " set_field %S, i32 %i\n"    // []
                  " load S %s\n"               // [addr]
                  " get_field %S, i32 %i\n"    // [1]
                  " load S %s\n"               // [1, addr]
                  " get_field %S, i32 %j\n"    // [1, 1]
                  " add i32\n"                 // [2]
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"                    // [addr1]
                  " dup S\n"                    // [addr1, addr1]
                  " const i32 1\n"              // [addr1, addr1, 1]
                  " set_field %S, i32 %i\n"     // [addr1]                              addr1.i = 1
                  " dup S\n"                    // [addr1, addr1]
                  " new S\n"                    // [addr1, addr1, addr2]
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"              // [addr1, addr1, addr2, addr2, 3]
                  " set_field %S, i32 %i\n"     // [addr1, addr1, addr2]                addr2.i = 3
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const_null\n"               // [addr1, addr1, addr2, addr2, null]
                  " set_field %S, S %next\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field %S, S %next\n"    // [addr1]                              addr1.next = addr2
                  " store S %s\n"               // []                                   s = addr1
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"                    // [addr1]
                  " dup S\n"                    // [addr1, addr1]
                  " const i32 1\n"              // [addr1, addr1, 1]
                  " set_field %S, i32 %i\n"     // [addr1]                              addr1.i = 1
                  " dup S\n"                    // [addr1, addr1]
                  " new S\n"                    // [addr1, addr1, addr2]
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"              // [addr1, addr1, addr2, addr2, 3]
                  " set_field %S, i32 %i\n"     // [addr1, addr1, addr2]                addr2.i = 3
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const_null\n"               // [addr1, addr1, addr2, addr2, null]
                  " set_field %S, S %next\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field %S, S %next\n"    // [addr1]                              addr1.next = addr2
                  " store S %s\n"               // []                                   s = addr1
                  " load S %s\n"                // [s]
                  " get_field %S, S %next\n"    // [s.next]
                  " get_field %S, i32 %i\n"     // [i]
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%S = type {\n"
                  " i32 %i,\n"
                  " S %next,\n"
                  "}\n"
                  "define i32 @test() {\n"
                  "local S %s\n"
                  "entry:\n"
                  " new S\n"                    // [addr1]
                  " dup S\n"                    // [addr1, addr1]
                  " const i32 1\n"              // [addr1, addr1, 1]
                  " set_field %S, i32 %i\n"     // [addr1]                              addr1.i = 1
                  " dup S\n"                    // [addr1, addr1]
                  " new S\n"                    // [addr1, addr1, addr2]
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const i32 3\n"              // [addr1, addr1, addr2, addr2, 3]
                  " set_field %S, i32 %i\n"     // [addr1, addr1, addr2]                addr2.i = 3
                  " dup S\n"                    // [addr1, addr1, addr2, addr2]
                  " const_null\n"               // [addr1, addr1, addr2, addr2, null]
                  " set_field %S, S %next\n"    // [addr1, addr1, addr2]                addr2.next = null
                  " set_field %S, S %next\n"    // [addr1]                              addr1.next = addr2
                  " store S %s\n"               // []                                   s = addr1
                  " load S %s\n"                // [s]
                  " get_field %S, S %next\n"    // [s.next]
                  " load S %s\n"                // [s.next, s]
                  " set_field %S, S %next\n"    // []                                   s.next.next = s
                  " load S %s\n"                // [s]
                  " get_field %S, S %next\n"    // [s.next]
                  " get_field %S, S %next\n"    // [s.next.next]
                  " const i32 2\n"              // [s.next.next, 2]
                  " set_field %S, i32 %i\n"     // []                                   s.next.next.i = 2
                  " load S %s\n"                // [s]
                  " get_field %S, i32 %i\n"     // [i]
                  " load S %s\n"                // [i, s]
                  " get_field %S, S %next\n"    // [i, s.next]
                  " get_field %S, i32 %i\n"     // [i, s.next.i]
                  " add i32\n"                  // [i + s.next.i]
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
        ty::context type_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        EXPECT_EQ(codegen_ctx.to_string(),
                  "%Link = type {\n"
                  " Link %next,\n"
                  "}\n"
                  "define void @test() {\n"
                  "local Link %root\n"
                  "entry:\n"
                  " new Link\n"                       // [addr1]
                  " dup Link\n"                       // [addr1, addr1]
                  " new Link\n"                       // [addr1, addr1, addr2]
                  " dup Link\n"                       // [addr1, addr1, addr2, addr2]
                  " const_null\n"                     // [addr1, addr1, addr2, addr2, null]
                  " set_field %Link, Link %next\n"    // [addr1, addr1, addr2]                   addr2.next = null
                  " set_field %Link, Link %next\n"    // [addr1]                                 addr1.next = addr2
                  " store Link %root\n"               // []                                      root = addr1
                  " load Link %root\n"                // [root]
                  " get_field %Link, Link %next\n"    // [root.next]
                  " load Link %root\n"                // [root.next, root]
                  " set_field %Link, Link %next\n"    // []                                      root.next.next = root
                  " load Link %root\n"                // [root]
                  " get_field %Link, Link %next\n"    // [root.next]
                  " const_null\n"                     // [root.next, null]
                  " set_field %Link, Link %next\n"    // []                                      root.next.next = null
                  " ret void\n"
                  "}");
    }
}

}    // namespace
