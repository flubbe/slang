/**
 * slang - a simple scripting language.
 *
 * Compiler output tests.
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
#include "emitter.h"
#include "typing.h"
#include "resolve.h"
#include "module.h"

#include "archives/file.h"

namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;

namespace
{

TEST(output, native_binding)
{
    {
        const std::string test_input =
          "/**\n"
          " * Print a string to stdout.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=slang)]\n"
          "fn print(s: str) -> void;\n"
          "\n"
          "/**\n"
          " * Print a string to stdout and append a new-line character.\n"
          " *\n"
          " * @param s The string to print.\n"
          " */\n"
          "#[native(lib=slang)]\n"
          "fn println(s: str) -> void;\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::language_module mod = emitter.to_module();
        slang::module_header header = mod.get_header();

        ASSERT_EQ(header.exports.size(), 2);
        EXPECT_EQ(header.imports.size(), 0);
        EXPECT_EQ(header.strings.size(), 0);

        ASSERT_EQ(header.exports[0].type, slang::symbol_type::function);
        EXPECT_EQ(header.exports[0].name, "print");
        {
            auto desc = std::get<slang::function_descriptor>(header.exports[0].desc);
            EXPECT_EQ(desc.native, true);
            EXPECT_EQ(desc.signature.return_type, "void");
            ASSERT_EQ(desc.signature.arg_types.size(), 1);
            EXPECT_EQ(desc.signature.arg_types[0], "str");
        }

        ASSERT_EQ(header.exports[1].type, slang::symbol_type::function);
        EXPECT_EQ(header.exports[1].name, "println");
        {
            auto desc = std::get<slang::function_descriptor>(header.exports[1].desc);
            EXPECT_EQ(desc.native, true);
            EXPECT_EQ(desc.signature.return_type, "void");
            ASSERT_EQ(desc.signature.arg_types.size(), 1);
            EXPECT_EQ(desc.signature.arg_types[0], "str");
        }

        {
            slang::file_write_archive write_ar("std.cmod");
            EXPECT_NO_THROW(write_ar & header);
        }

        {
            slang::module_header read_header;
            slang::file_read_archive read_ar("std.cmod");
            ASSERT_NO_THROW(read_ar & read_header);

            EXPECT_EQ(read_header.exports.size(), header.exports.size());
            EXPECT_EQ(read_header.imports.size(), header.imports.size());
            EXPECT_EQ(read_header.strings.size(), header.strings.size());

            ASSERT_EQ(read_header.exports.size(), 2);

            ASSERT_EQ(read_header.exports[0].type, slang::symbol_type::function);
            EXPECT_EQ(read_header.exports[0].name, "print");
            {
                auto desc = std::get<slang::function_descriptor>(read_header.exports[0].desc);
                EXPECT_EQ(desc.native, true);
                EXPECT_EQ(desc.signature.return_type, "void");
                ASSERT_EQ(desc.signature.arg_types.size(), 1);
                EXPECT_EQ(desc.signature.arg_types[0], "str");
            }

            ASSERT_EQ(read_header.exports[1].type, slang::symbol_type::function);
            EXPECT_EQ(read_header.exports[1].name, "println");
            {
                auto desc = std::get<slang::function_descriptor>(read_header.exports[1].desc);
                EXPECT_EQ(desc.native, true);
                EXPECT_EQ(desc.signature.return_type, "void");
                ASSERT_EQ(desc.signature.arg_types.size(), 1);
                EXPECT_EQ(desc.signature.arg_types[0], "str");
            }
        }
    }
}

TEST(output, emitter)
{
    {
        const std::string test_input =
          /* i32 */
          "fn itest() -> i32 {\n"
          " return 1;\n"
          "}\n"
          "fn iadd() -> i32 {\n"
          " return 1+2;\n"
          "}\n"
          "fn isub() -> i32 {\n"
          " return 3-2;\n"
          "}\n"
          "fn imul() -> i32 {\n"
          " return 2*3;\n"
          "}\n"
          "fn idiv() -> i32 {\n"
          " return 6 / 2;\n"
          "}\n"
          /* f32 */
          "fn ftest() -> f32 {\n"
          " return 1.1;\n"
          "}\n"
          "fn fadd() -> f32 {\n"
          " return 1.1+2.1;\n"
          "}\n"
          "fn fsub() -> f32 {\n"
          " return 3.1-2.1;\n"
          "}\n"
          "fn fmul() -> f32 {\n"
          " return 2.1*3.1;\n"
          "}\n"
          "fn fdiv() -> f32 {\n"
          " return 6.4 / 2.0;\n"
          "}\n"
          /* str */
          "fn stest() -> str {\n"
          " return \"Test\";\n"
          "}\n"
          /* arguments */
          "fn arg(a: i32) -> i32 {\n"
          " return 1 + a;\n"
          "}\n"
          "fn arg2(a: f32) -> f32 {\n"
          " return 2.0*a+1.0;\n"
          "}\n"
          "fn sid(a: str) -> str {\n"
          " return a;\n"
          "}\n"
          /* function calls */
          "fn call(a: i32) -> i32 {\n"
          " return arg(a) - 1;\n"
          "}\n"
          /* locals. */
          "fn local(a: i32) -> i32 {\n"
          " let b: i32 = -1;\n"
          " return a+b;\n"
          "}\n"
          "fn local2(a: i32) -> i32 {\n"
          " let b: i32 = -1;\n"
          " return a-b;\n"
          "}\n"
          /* Type casts. */
          "fn cast_i2f(a: i32) -> f32 {\n"
          " return a as f32;\n"
          "}\n"
          "fn cast_f2i(a: f32) -> i32 {\n"
          " return a as i32;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("test_output.bin");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        slang::language_module mod;
        slang::file_read_archive read_ar("test_output.bin");
        EXPECT_NO_THROW(read_ar & mod);
    }
}

TEST(output, hello_world)
{
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(s: str) -> i32\n"
          "{\n"
          "\tstd::println(\"Hello, World!\");\n"
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("src/lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("hello_world.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "import lang::std;\n"
          "\n"
          "fn main(s: str) -> i32\n"
          "{\n"
          "\tlang::println(\"Hello, World!\");\n"    // wrong function path
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("src");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(output, operators)
{
    const std::string test_input =
      "fn main() -> i32\n"
      "{\n"
      "\tlet a: i32 = 1 + 2;\n"
      "\ta = 1 - 2;\n"
      "\ta = 1 * 2;\n"
      "\ta = 1 / 2;\n"
      "\ta += -1;\n"
      "\ta -= -2;\n"
      "\ta *= -3;\n"
      "\ta /= -2;\n"
      "\ta %= 1;\n"
      "\tlet b: f32 = 1.0 + 2.0;\n"
      "\tb = 1.0 - 2.0;\n"
      "\tb = 1.0 * 2.0;\n"
      "\tb = 1.0 / 2.0;\n"
      "\tlet c: i32 = 1 & 2;\n"
      "\tc = 1 | 2;\n"
      "\tc = 1 ^ 2;\n"
      "\tc = 1 << 2;\n"
      "\tc = 1 >> 2;\n"
      "\treturn 0;\n"
      "}\n"
      "fn and(a: i32, b: i32) -> i32 { return a & b; }\n"
      "fn or(a: i32, b: i32) -> i32 { return a | b; }\n"
      "fn xor(a: i32, b: i32) -> i32 { return a ^ b; }\n"
      "fn shl(a: i32, b: i32) -> i32 { return a << b; }\n"
      "fn shr(a: i32, b: i32) -> i32 { return a >> b; }\n"
      "fn mod(a: i32, b: i32) -> i32 { return a % b; }\n";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    const slang::ast::block* ast = parser.get_ast();
    ASSERT_NE(ast, nullptr);

    slang::file_manager mgr;
    mgr.add_search_path("src/lang");

    ty::context type_ctx;
    rs::context resolve_ctx{mgr};
    cg::context codegen_ctx;
    slang::instruction_emitter emitter{codegen_ctx};

    ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(ast->type_check(type_ctx));
    ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    ASSERT_NO_THROW(emitter.run());

    slang::language_module mod = emitter.to_module();

    slang::file_write_archive write_ar("operators.cmod");
    EXPECT_NO_THROW(write_ar & mod);
}

TEST(output, control_flow)
{
    const std::string test_input =
      "import std;\n"
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
      "}\n"
      "fn conditional_hello_world(a: f32) -> void\n"
      "{\n"
      " if(a > 2.5)\n"
      " {\n"
      "  std::println(\"Hello, World!\");\n"
      " }\n"
      " else\n"
      " {\n"
      "  std::println(\"World, hello!\");\n"
      " }\n"
      "}\n"
      "fn no_else(a: i32) -> void\n"
      "{\n"
      " if(a > 0)\n"
      " {\n"
      "  std::println(\"a>0\");\n"
      " }\n"
      " std::println(\"Test\");\n"
      "}\n";

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    parser.parse(lexer);

    EXPECT_TRUE(lexer.eof());

    const slang::ast::block* ast = parser.get_ast();
    ASSERT_NE(ast, nullptr);

    slang::file_manager mgr;
    mgr.add_search_path("src/lang");

    ty::context type_ctx;
    rs::context resolve_ctx{mgr};
    cg::context codegen_ctx;
    slang::instruction_emitter emitter{codegen_ctx};

    ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(ast->type_check(type_ctx));
    ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    ASSERT_NO_THROW(emitter.run());

    slang::language_module mod = emitter.to_module();

    slang::file_write_archive write_ar("control_flow.cmod");
    EXPECT_NO_THROW(write_ar & mod);
}

TEST(output, loops)
{
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main() -> void\n"
          "{\n"
          " let i: i32 = 0;\n"
          " while(i < 10)\n"
          " {\n"
          "  std::println(\"Hello, World!\");\n"
          "  i += 1;\n"
          " }\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        const slang::ast::block* ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("src/lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("loops.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

}    // namespace