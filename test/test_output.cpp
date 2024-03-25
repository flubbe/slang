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

        ty::context type_ctx;
        rs::context resolve_ctx;
        cg::context codegen_ctx;

        ASSERT_NO_THROW(ast->collect_names(type_ctx));
        ASSERT_NO_THROW(resolve_ctx.resolve_imports(type_ctx));
        ASSERT_NO_THROW(ast->type_check(type_ctx));
        ASSERT_NO_THROW(ast->generate_code(&codegen_ctx));
        ASSERT_NO_THROW(codegen_ctx.finalize());

        slang::language_module mod = codegen_ctx.to_module();
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
            slang::file_write_archive write_ar("test_output.bin");
            EXPECT_NO_THROW(write_ar & header);
        }

        {
            slang::module_header read_header;
            slang::file_read_archive read_ar("test_output.bin");
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

}    // namespace