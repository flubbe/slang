/**
 * slang - a simple scripting language.
 *
 * `compile` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <print>

#include "compiler/codegen/codegen.h"
#include "compiler/emitter.h"
#include "compiler/macro.h"
#include "compiler/opt/cfg.h"
#include "compiler/parser.h"
#include "compiler/resolve.h"
#include "compiler/typing.h"
#include "commandline.h"
#include "loader.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace ty = slang::typing;
namespace ld = slang::loader;
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
            std::println("Info: Module path: {}", module_path.string());
        }

        // generate output file name, if necessary.
        if(!has_output)
        {
            output_file = module_path;
            output_file.replace_extension(package::module_ext);

            if(verbose)
            {
                std::println("Info: Output file: {}", output_file.string());
            }
        }
    }

    if(display_help_and_exit)
    {
        std::println("{}", options.help());
        return;
    }

    // Flags.

    bool evaluate_constant_subexpressions = result.count("no-eval-const-subexpr") > 0;
    if(verbose)
    {
        if(!evaluate_constant_subexpressions)
        {
            std::println("Info: Evaluation of constant subexpressions disabled.");
        }
        else
        {
            std::println("Info: Evaluation of constant subexpressions enabled (default).");
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
            std::println("Info: Adding 'lang' to search paths.");
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
                std::println("Info: Adding '{}' to search paths.", it);
            }

            file_mgr.add_search_path(it);
        }
    }

    // Compile.

    if(!file_mgr.is_file(module_path))
    {
        throw std::runtime_error(std::format("Module '{}' does not exist.", module_path.string()));
    }

    std::println("Compiling '{}'...", module_path.string());

    std::string input_buffer;
    {
        auto input_ar = file_mgr.open(module_path, slang::file_manager::open_mode::read);
        std::size_t input_size = input_ar->size();
        if(input_size == 0)
        {
            std::println("Empty input.");
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
        std::println("Lexer did not complete input reading.");
        return;
    }

    std::shared_ptr<ast::expression> ast = parser.get_ast();
    if(!ast)
    {
        std::println("No AST produced.");
        return;
    }

    const std::vector<ast::expression*> module_macro_asts = parser.get_macro_asts();

    ld::context loader_ctx{file_mgr};
    sema::env sema_env;
    const_::env const_env;
    ty::context type_ctx;
    co::context co_ctx{sema_env};
    rs::context resolver_ctx{sema_env, type_ctx};
    tl::context lowering_ctx{type_ctx};
    cg::context codegen_ctx{sema_env, const_env, lowering_ctx};
    macro::env macro_env;
    opt::cfg::context cfg_context{codegen_ctx};
    slang::instruction_emitter emitter{
      sema_env,
      const_env,
      macro_env,
      type_ctx,
      codegen_ctx};
    codegen_ctx.evaluate_constant_subexpressions = evaluate_constant_subexpressions;

    ast->collect_names(co_ctx);
    ast->collect_attributes(sema_env);

    resolver_ctx.resolve_imports(loader_ctx);
    ast->resolve_names(resolver_ctx);

    ast->collect_macros(sema_env, macro_env);
    do    // NOLINT(cppcoreguidelines-avoid-do-while)
    {
        do    // NOLINT(cppcoreguidelines-avoid-do-while)
        {
            resolver_ctx.resolve_imports(loader_ctx);
        } while(ld::context::resolve_macros(macro_env, type_ctx));
    } while(ast->expand_macros(codegen_ctx, type_ctx, macro_env, module_macro_asts));

    ast->declare_types(type_ctx, sema_env);
    ast->define_types(type_ctx);
    ast->declare_functions(type_ctx, sema_env);
    ast->bind_constant_declarations(sema_env, const_env);
    ast->type_check(type_ctx, sema_env);
    ast->evaluate_constant_expressions(type_ctx, const_env);
    ast->generate_code(codegen_ctx);

    cfg_context.run();
    emitter.run();

    slang::module_::language_module mod = emitter.to_module();

    slang::file_write_archive write_ar(output_file.string());
    write_ar & mod;

    std::println("Compilation finished. Output file: {}", output_file.string());
}

std::string compile::get_description() const
{
    return "Compile a module.";
}

}    // namespace slang::commandline
