/**
 * slang - a simple scripting language.
 *
 * `compile` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "compiler/codegen.h"
#include "compiler/emitter.h"
#include "compiler/parser.h"
#include "compiler/typing.h"
#include "commandline.h"
#include "resolve.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;

namespace slang::commandline
{

compile::compile(slang::package_manager& manager)
: command{"compile"}
, manager{manager}
{
}

void compile::invoke(const std::vector<std::string>& args)
{
    bool display_help_and_exit = false;

    cxxopts::Options options = make_cxxopts_options();

    // clang-format off
    options.add_options()
        ("v,verbose", "Verbose output.")
        ("o,output", "Output file.", cxxopts::value<std::string>())
        ("no-lang", "Exclude default language modules.")
        ("search-path", "Additional search paths for module resolution, separated by ';'.", cxxopts::value<std::string>())
        ("no-eval-const-subexpr", "Disable constant subexpression evaluation")
        ("filename", "The file to compile", cxxopts::value<std::string>());
    // clang-format on

    options.parse_positional({"filename"});
    options.positional_help("filename");
    auto result = parse_args(options, args);

    if(result.count("filename") < 1)
    {
        display_help_and_exit = true;
    }

    bool verbose = result.count("verbose") > 0;

    fs::path module_path;
    fs::path output_file;

    if(!display_help_and_exit)
    {
        // get output file.
        bool has_output = result.count("output") > 0;
        if(has_output)
        {
            output_file = result["output"].as<std::string>();
            if(!output_file.has_extension())
            {
                output_file.replace_extension(package::module_ext);
            }
        }

        // get input file.
        module_path = result["filename"].as<std::string>();
        if(!module_path.has_extension())
        {
            module_path.replace_extension(package::source_ext);
        }

        if(verbose)
        {
            fmt::print("Info: Module path: {}\n", module_path.string());
        }

        // generate output file name, if necessary.
        if(!has_output)
        {
            output_file = module_path;
            output_file.replace_extension(package::module_ext);

            if(verbose)
            {
                fmt::print("Info: Output file: {}\n", output_file.string());
            }
        }
    }

    if(display_help_and_exit)
    {
        fmt::print("{}\n", options.help());
        return;
    }

    // Flags.

    bool evaluate_constant_subexpressions = result.count("no-eval-const-subexpr") > 0;
    if(verbose)
    {
        if(!evaluate_constant_subexpressions)
        {
            fmt::print("Info: Evaluation of constant subexpressions disabled.\n");
        }
        else
        {
            fmt::print("Info: Evaluation of constant subexpressions enabled (default).\n");
        }
    }

    // Add search paths.
    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");

    bool no_lang = result.count("no-lang") > 0;

    if(!no_lang)
    {
        if(verbose)
        {
            fmt::print("Info: Adding 'lang' to search paths.\n");
        }

        file_mgr.add_search_path("lang");
    }

    if(result.count("search-path") > 0)
    {
        std::vector<std::string> v = utils::split(result["search-path"].as<std::string>(), ";");
        for(auto& it: v)
        {
            if(verbose)
            {
                fmt::print("Info: Adding '{}' to search paths.\n", it);
            }

            file_mgr.add_search_path(it);
        }
    }

    // Compile.

    if(!file_mgr.is_file(module_path))
    {
        throw std::runtime_error(fmt::format("Module '{}' does not exist.", module_path.string()));
    }

    fmt::print("Compiling '{}'...\n", module_path.string());

    std::string input_buffer;
    {
        auto input_ar = file_mgr.open(module_path, slang::file_manager::open_mode::read);
        std::size_t input_size = input_ar->size();
        if(input_size == 0)
        {
            fmt::print("Empty input.\n");
            return;
        }

        input_buffer.resize(input_size);
        input_ar->serialize(reinterpret_cast<std::byte*>(input_buffer.data()), input_size);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(input_buffer);
    parser.parse(lexer);

    if(!lexer.eof())
    {
        fmt::print("Lexer did not complete input reading.\n");
        return;
    }

    std::shared_ptr<ast::expression> ast = parser.get_ast();
    if(!ast)
    {
        fmt::print("No AST produced.\n");
        return;
    }

    const std::vector<ast::expression*> module_macro_asts = parser.get_macro_asts();

    ty::context type_ctx;
    rs::context resolve_ctx{file_mgr};
    cg::context codegen_ctx;
    slang::instruction_emitter emitter{codegen_ctx};

    codegen_ctx.evaluate_constant_subexpressions = evaluate_constant_subexpressions;

    ast->collect_names(codegen_ctx, type_ctx);
    do    // NOLINT(cppcoreguidelines-avoid-do-while)
    {
        do    // NOLINT(cppcoreguidelines-avoid-do-while)
        {
            resolve_ctx.resolve_imports(codegen_ctx, type_ctx);
        } while(rs::context::resolve_macros(codegen_ctx, type_ctx));
    } while(ast->expand_macros(codegen_ctx, module_macro_asts));
    type_ctx.resolve_types();
    ast->type_check(type_ctx);
    ast->generate_code(codegen_ctx);

    emitter.run();

    slang::module_::language_module mod = emitter.to_module();

    slang::file_write_archive write_ar(output_file.string());
    write_ar & mod;

    fmt::print("Compilation finished. Output file: {}\n", output_file.string());
}

std::string compile::get_description() const
{
    return "Compile a module.";
}

}    // namespace slang::commandline
