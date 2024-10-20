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

namespace ast = slang::ast;
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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();
        slang::module_::module_header header = mod.get_header();

        ASSERT_EQ(header.exports.size(), 2);
        EXPECT_EQ(header.imports.size(), 0);
        EXPECT_EQ(header.strings.size(), 0);

        ASSERT_EQ(header.exports[0].type, slang::module_::symbol_type::function);
        EXPECT_EQ(header.exports[0].name, "print");
        {
            auto desc = std::get<slang::module_::function_descriptor>(header.exports[0].desc);
            EXPECT_EQ(desc.native, true);
            EXPECT_EQ(desc.signature.return_type.first.base_type(), "void");
            EXPECT_EQ(desc.signature.return_type.second, false);
            ASSERT_EQ(desc.signature.arg_types.size(), 1);
            EXPECT_EQ(desc.signature.arg_types[0].first.base_type(), "str");
            EXPECT_EQ(desc.signature.arg_types[0].second, false);
        }

        ASSERT_EQ(header.exports[1].type, slang::module_::symbol_type::function);
        EXPECT_EQ(header.exports[1].name, "println");
        {
            auto desc = std::get<slang::module_::function_descriptor>(header.exports[1].desc);
            EXPECT_EQ(desc.native, true);
            EXPECT_EQ(desc.signature.return_type.first.base_type(), "void");
            EXPECT_EQ(desc.signature.return_type.second, false);
            ASSERT_EQ(desc.signature.arg_types.size(), 1);
            EXPECT_EQ(desc.signature.arg_types[0].first.base_type(), "str");
            EXPECT_EQ(desc.signature.arg_types[0].second, false);
        }

        {
            slang::file_write_archive write_ar("native_binding.cmod");
            EXPECT_NO_THROW(write_ar & header);
        }

        {
            slang::module_::module_header read_header;
            slang::file_read_archive read_ar("native_binding.cmod");
            ASSERT_NO_THROW(read_ar & read_header);

            EXPECT_EQ(read_header.exports.size(), header.exports.size());
            EXPECT_EQ(read_header.imports.size(), header.imports.size());
            EXPECT_EQ(read_header.strings.size(), header.strings.size());

            ASSERT_EQ(read_header.exports.size(), 2);

            ASSERT_EQ(read_header.exports[0].type, slang::module_::symbol_type::function);
            EXPECT_EQ(read_header.exports[0].name, "print");
            {
                auto desc = std::get<slang::module_::function_descriptor>(read_header.exports[0].desc);
                EXPECT_EQ(desc.native, true);
                EXPECT_EQ(desc.signature.return_type.first.base_type(), "void");
                EXPECT_EQ(desc.signature.return_type.second, false);
                ASSERT_EQ(desc.signature.arg_types.size(), 1);
                EXPECT_EQ(desc.signature.arg_types[0].first.base_type(), "str");
                EXPECT_EQ(desc.signature.arg_types[0].second, false);
            }

            ASSERT_EQ(read_header.exports[1].type, slang::module_::symbol_type::function);
            EXPECT_EQ(read_header.exports[1].name, "println");
            {
                auto desc = std::get<slang::module_::function_descriptor>(read_header.exports[1].desc);
                EXPECT_EQ(desc.native, true);
                EXPECT_EQ(desc.signature.return_type.first.base_type(), "void");
                EXPECT_EQ(desc.signature.return_type.second, false);
                ASSERT_EQ(desc.signature.arg_types.size(), 1);
                EXPECT_EQ(desc.signature.arg_types[0].first.base_type(), "str");
                EXPECT_EQ(desc.signature.arg_types[0].second, false);
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
          "fn arg3(a: f32, s: str) -> f32 {\n"
          " s = \"Test\";\n"
          " return 2.0 + a;\n"
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
          "fn local3() -> str {\n"
          " let s: str = \"Test\";\n"
          " return s;\n"
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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("test_output.bin");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        slang::module_::language_module mod;
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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("hello_world.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main(args: [str]) -> i32\n"
          "{\n"
          "\tlang::println(\"Hello, World!\");\n"    // wrong function path
          "\treturn 0;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(output, operators)
{
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
          "fn land(a: i32, b: i32) -> i32 { return a && b; }\n"
          "fn or(a: i32, b: i32) -> i32 { return a | b; }\n"
          "fn lor(a: i32, b: i32) -> i32 { return a || b; }\n"
          "fn xor(a: i32, b: i32) -> i32 { return a ^ b; }\n"
          "fn shl(a: i32, b: i32) -> i32 { return a << b; }\n"
          "fn shr(a: i32, b: i32) -> i32 { return a >> b; }\n"
          "fn mod(a: i32, b: i32) -> i32 { return a % b; }\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("operators.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn main() -> [i32]\n"
          "{\n"
          "\treturn 1 + 2;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        EXPECT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(output, string_operations)
{
    {
        const std::string test_input =
          "#[native(lib=\"slang\")]\n"
          "fn string_equals(s1: str, s2: str) -> i32;\n"
          "#[native(lib=\"slang\")]\n"
          "fn string_concat(s1: str, s2: str) -> str;\n"
          "fn main() -> i32\n"
          "{\n"
          "\tlet s: str = string_concat(\"a\", \"b\");\n"
          "\tif(string_equals(s, \"ab\"))\n"
          "\t{\n"
          "\t\treturn 10;\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("string_operations.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn main() -> void\n"
          "{\n"
          "\tlet c: str = \"a\" + \"b\";\n"    // cannot add strings
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(output, prefix_postfix)
{
    {
        const std::string test_input =
          "fn prefix_add_i32(i: i32) -> i32\n"
          "{\n"
          "\treturn ++i;\n"
          "}\n"
          "fn prefix_sub_i32(i: i32) -> i32\n"
          "{\n"
          "\treturn --i;\n"
          "}\n"
          "fn postfix_add_i32(i: i32) -> i32\n"
          "{\n"
          "\treturn i++;\n"
          "}\n"
          "fn postfix_sub_i32(i: i32) -> i32\n"
          "{\n"
          "\treturn i--;\n"
          "}\n"
          "fn prefix_add_f32(i: f32) -> f32\n"
          "{\n"
          "\treturn ++i;\n"
          "}\n"
          "fn prefix_sub_f32(i: f32) -> f32\n"
          "{\n"
          "\treturn --i;\n"
          "}\n"
          "fn postfix_add_f32(i: f32) -> f32\n"
          "{\n"
          "\treturn i++;\n"
          "}\n"
          "fn postfix_sub_f32(i: f32) -> f32\n"
          "{\n"
          "\treturn i--;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("prefix_postfix.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
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

    std::shared_ptr<ast::block> ast = parser.get_ast();
    ASSERT_NE(ast, nullptr);

    slang::file_manager mgr;
    mgr.add_search_path("lang");

    ty::context type_ctx;
    rs::context resolve_ctx{mgr};
    cg::context codegen_ctx;
    slang::instruction_emitter emitter{codegen_ctx};

    ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(type_ctx.resolve_types());
    ASSERT_NO_THROW(ast->type_check(type_ctx));
    ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
    ASSERT_NO_THROW(emitter.run());

    slang::module_::language_module mod = emitter.to_module();

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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("loops.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "import std;\n"
          "\n"
          "fn main_b() -> void\n"
          "{\n"
          " let i: i32 = 0;\n"
          " while(i < 10)\n"
          " {\n"
          "  std::println(\"Hello, World!\");\n"
          "  i += 1;\n"
          "  break;\n"
          " }\n"
          "}\n"
          "fn main_c() -> void\n"
          "{\n"
          " let i: i32 = 0;\n"
          " while(i < 10)\n"
          " {\n"
          "  std::println(\"Hello, World!\");\n"
          "  i = 10;\n"
          "  continue;\n"
          "  i = 1;\n"
          " }\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path("lang");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("loops_bc.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, infinite_recursion)
{
    {
        const std::string test_input =
          "fn inf() -> void\n"
          "{\n"
          " inf();\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("inf_recursion.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, arrays)
{
    {
        const std::string test_input =
          "fn f() -> i32\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          " return b[1];\n"
          "}\n"
          "fn g() -> i32\n"
          "{\n"
          " let b: [i32] = [-1, 0, f()];\n"
          " b[1] = 3;\n"
          " return b[1];\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("arrays.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn return_array() -> [i32]\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          " return b;\n"
          "}\n"
          "fn pass_array() -> i32\n"
          "{\n"
          " let b: [i32] = [2, 3];\n"
          " return f(b);\n"
          "}\n"
          "fn f(a: [i32]) -> i32\n"
          "{\n"
          " return a[1];\n"
          "}\n"
          "fn invalid_index() -> i32\n"
          "{\n"
          " let b: [i32] = [0, 1];\n"
          " return b[3];\n"
          "}\n"
          "fn str_array() -> [str]\n"
          "{\n"
          " let s: [str] = [\"a\", \"test\", \"123\"];"
          " return s;\n"
          "}\n"
          "fn ret_str() -> str\n"
          "{\n"
          " let s: [str] = [\"a\", \"test\", \"123\"];"
          " return s[2];"
          "}\n"
          "fn call_return() -> i32\n"
          "{\n"
          " return return_array()[0];\n"
          "}\n"
          "fn new_array() -> void\n"
          "{\n"
          " let b: [i32] = new i32[2];\n"
          " b[0] = 1;\n"
          " b[1] = 10;\n"
          "}\n"
          "fn new_array_invalid_size() -> void\n"
          "{\n"
          " let b: [i32] = new i32[-1];\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("return_array.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn return_array() -> [i32]\n"
          "{\n"
          " let b: [i32] = [1, 2];\n"
          " return b[0];\n"    // wrong return type
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn return_array() -> i32\n"
          "{\n"
          " let b: i32 = 1;\n"
          " return b[0];\n"    // not an array
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn array_init_wrong_type() -> i32\n"
          "{\n"
          " let b: i32 = [2, 3];\n"    // not an array.
          " return b;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
    {
        const std::string test_input =
          "fn len() -> i32\n"
          "{\n"
          " let b: [i32] = [2, 3];\n"
          " return b.length;\n"
          "}\n"
          "fn len2() -> i32\n"
          "{\n"
          " let b: [i32];\n"
          " return b.length;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("array_length.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "#[native(lib=\"slang\")]\n"
          "fn array_copy(from: [], to: []) -> void;\n"
          "#[native(lib=\"slang\")]\n"
          "fn string_equals(s1: str, s2: str) -> i32;\n"
          "fn test_copy() -> i32\n"
          "{\n"
          " let a: [i32] = [2, 3];\n"
          " let b: [i32] = new i32[2];\n"
          " array_copy(a, b);\n"
          " return a.length == b.length && a[0] == b[0] && a[1] == b[1];\n"
          "}\n"
          "fn test_copy_str() -> i32\n"
          "{\n"
          " let a: [str] = [\"a\", \"123\"];\n"
          " let b: [str] = new str[2];\n"
          " array_copy(a, b);\n"
          " return a.length == b.length && string_equals(a[0], b[0]) && string_equals(a[1], b[1]);\n"
          "}\n"
          "fn test_copy_fail_none() -> void\n"
          "{\n"
          " let a: [i32] = [2, 3];\n"
          " let b: [i32];\n"    // no target array
          " array_copy(a, b);\n"
          "}\n"
          "fn test_copy_fail_type() -> void\n"
          "{\n"
          " let a: [i32] = [2, 3];\n"
          " let b: [f32] = new f32[2];\n"    // wrong type
          " array_copy(a, b);\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("array_copy.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn array_copy(from: [], to: []) -> void;";    // need array type.

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        ASSERT_THROW(parser.parse(lexer), slang::syntax_error);
    }
}

TEST(output, return_discard)
{
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> i32\n"
          "{\n"
          " return 123;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        ASSERT_EQ(codegen_ctx.to_string(),
                  "define void @f() {\n"
                  "entry:\n"
                  " invoke @g\n"
                  " pop i32\n"
                  " ret void\n"
                  "}\n"
                  "define i32 @g() {\n"
                  "entry:\n"
                  " const i32 123\n"
                  " ret i32\n"
                  "}");

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("return_discard.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> [i32]\n"
          "{\n"
          " let r: [i32] = [1, 2];\n"
          " return r;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("return_discard_array.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "fn f() -> void\n"
          "{\n"
          " g();\n"
          "}\n"
          "fn g() -> [str]\n"
          "{\n"
          " let r: [str] = [\"a\", \"test\"];\n"
          " return r;\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("return_discard_strings.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, missing_return)
{
    {
        // missing return statement
        const std::string test_input =
          "fn g() -> i32\n"
          "{\n"
          "}";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_THROW(ast->generate_code(codegen_ctx), cg::codegen_error);
    }
}

TEST(output, structs)
{
    {
        const std::string test_input =
          "struct S {\n"
          " i: i32,\n"
          " j: f32\n"
          "};\n"
          "struct T{\n"
          " s: S,\n"
          " t: str\n"
          "};";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("structs.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("structs_access.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
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
          "}\n"
          "fn test_local() -> i32\n"
          "{\n"
          " let s: S = S{ i: 2, j: 3 };\n"
          " let i: i32 = s.j = 1;\n"
          " return i + s.i + s.j;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("structs_access2.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "struct S {\n"
          " s: S\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = S{s: null};\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("structs_references.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "struct S {\n"
          " i: i32\n"
          "};\n"
          "fn test() -> void\n"
          "{\n"
          " let s: S = null;\n"
          " s.i = 10;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("null_dereference.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, nested_structs)
{
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

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("nested_structs.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
    {
        const std::string test_input =
          "struct Data {\n"
          " i: i32,\n"
          " f: f32,\n"
          " s: str,\n"
          " next: Data"
          "};"
          "struct Container {\n"
          " data: Data,\n"
          " flags: i32\n"
          "};\n"
          "fn test() -> i32\n"
          "{\n"
          " let c: Container = Container{\n"
          "  data: Data{i: -1, f: 3.14, s: \"Test\", next: null},\n"
          "  flags: 4096\n"
          " };\n"
          " return c.data.i + (c.data.f as i32);\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("nested_structs2.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, type_imports)
{
    {
        const std::string test_input =
          "import nested_structs2;\n"
          "fn test() -> i32\n"
          "{\n"
          " let c: nested_structs2::Container = nested_structs2::Container{\n"
          "  data: nested_structs2::Data{i: -1, f: 3.14, s: \"Test\", next: null},\n"
          "  flags: 4096\n"
          " };\n"
          " return c.data.i + (c.data.f as i32);\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;
        mgr.add_search_path(".");

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(codegen_ctx));

        ASSERT_NO_THROW(emitter.run());

        slang::module_::language_module mod = emitter.to_module();

        slang::file_write_archive write_ar("type_imports.cmod");
        EXPECT_NO_THROW(write_ar & mod);
    }
}

TEST(output, null_assignment)
{
    {
        const std::string test_input =
          "fn test() -> void\n"
          "{\n"
          " let s: i32 = null;\n"
          "}\n";

        slang::lexer lexer;
        slang::parser parser;

        lexer.set_input(test_input);
        parser.parse(lexer);

        EXPECT_TRUE(lexer.eof());

        std::shared_ptr<ast::block> ast = parser.get_ast();
        ASSERT_NE(ast, nullptr);

        slang::file_manager mgr;

        ty::context type_ctx;
        rs::context resolve_ctx{mgr};
        cg::context codegen_ctx;
        slang::instruction_emitter emitter{codegen_ctx};

        ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
        ASSERT_NO_THROW(type_ctx.resolve_types());
        ASSERT_THROW(ast->type_check(type_ctx), ty::type_error);
    }
}

TEST(output, multiple_modules)
{
    {
        const std::pair<std::string, std::string> module_inputs[] = {
          {"mod1",
           "fn f() -> i32 { return 2; }"},
          {"mod2",
           "import mod1;\n"
           "fn f(x: i32) -> f32 { return (mod1::f() * x) as f32; }\n"},
          {"mod3",
           "import mod2;\n"
           "fn f(x: f32) -> i32 { return (mod2::f(x as i32) * 2.0) as i32; }\n"}};

        for(auto& s: module_inputs)
        {
            slang::lexer lexer;
            slang::parser parser;

            lexer.set_input(s.second);
            parser.parse(lexer);

            EXPECT_TRUE(lexer.eof());

            std::shared_ptr<ast::block> ast = parser.get_ast();
            ASSERT_NE(ast, nullptr);

            slang::file_manager mgr;
            mgr.add_search_path(".");

            ty::context type_ctx;
            rs::context resolve_ctx{mgr};
            cg::context codegen_ctx;
            slang::instruction_emitter emitter{codegen_ctx};

            ASSERT_NO_THROW(ast->collect_names(codegen_ctx, type_ctx));
            ASSERT_NO_THROW(resolve_ctx.resolve_imports(codegen_ctx, type_ctx));
            ASSERT_NO_THROW(type_ctx.resolve_types());
            ASSERT_NO_THROW(ast->type_check(type_ctx));
            ASSERT_NO_THROW(ast->generate_code(codegen_ctx));
            ASSERT_NO_THROW(emitter.run());

            slang::module_::language_module mod = emitter.to_module();

            slang::file_write_archive write_ar(fmt::format("{}.cmod", s.first));
            EXPECT_NO_THROW(write_ar & mod);
        }
    }
}

}    // namespace
