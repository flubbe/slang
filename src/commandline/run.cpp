/**
 * slang - a simple scripting language.
 *
 * `run` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <print>

#include "archives/file.h"
#include "interpreter/interpreter.h"
#include "interpreter/invoke.h"
#include "runtime/runtime.h"
#include "shared/module.h"
#include "commandline.h"
#include "utils.h"

namespace rt = slang::runtime;
namespace si = slang::interpreter;

namespace slang::commandline
{

/*
 * run implementation.
 */

/**
 * Validate the signature of `main`.
 *
 * @param main_function The main function to validate the signature of.
 * @param verbose Whether to enable verbose logging.
 * @throws Throws a `std::runtime_error` if validation failed.
 */
static void validate_main_signature(const si::function& main_function, bool verbose)
{
    if(verbose)
    {
        std::print("Info: Validating signature of 'main'.\n");
    }

    // validate function signature for 'main'.
    const module_::function_signature& sig = main_function.get_signature();
    if(sig.return_type.base_type() != "i32" || sig.return_type.is_array())
    {
        throw std::runtime_error(std::format(
          "Invalid return type for 'main' expected 'i32', got '{}'.",
          slang::module_::to_string(sig.return_type)));
    }
    if(sig.arg_types.size() != 1)
    {
        throw std::runtime_error(std::format(
          "Invalid parameter count for 'main'. Expected 1 parameter, got {}.",
          sig.arg_types.size()));
    }
    if(sig.arg_types[0].base_type() != "str" || !sig.arg_types[0].is_array())
    {
        throw std::runtime_error(std::format(
          "Invalid parameter type for 'main'. Expected parameter of type '[str]', got '{}'.",
          slang::module_::to_string(sig.arg_types[0])));
    }
}

/**
 * Check if the garbage collector is finalized / cleaned up.
 *
 * @param gc The garbage collector.
 * @param verbose Whether to enable verbose logging.
 */
static void check_finalized_gc(gc::garbage_collector& gc, bool verbose)
{
    if(verbose)
    {
        std::print("Info: Checking GC cleanup.\n");
    }

    if(gc.object_count() != 0)
    {
        std::print("GC warning: Object count is {}.\n", gc.object_count());
    }
    if(gc.root_set_size() != 0)
    {
        std::print("GC warning: Root set size is {}.\n", gc.root_set_size());
    }
    if(gc.byte_size() != 0)
    {
        std::print("GC warning: {} bytes still allocated.\n", gc.byte_size());
    }
}

run::run(slang::package_manager& manager)
: command{"run"}
, manager{manager}
{
}

void run::invoke(const std::vector<std::string>& args)
{
    cxxopts::Options options = make_cxxopts_options();

    // clang-format off
    options.add_options()
        ("v,verbose", "Verbose output.")
        ("no-lang", "Exclude default language modules.")
        ("search-path", "Additional search paths for module resolution, separated by ';'.", cxxopts::value<std::string>())
        ("filename", "The file to run.", cxxopts::value<std::string>());
    // clang-format on

    options.parse_positional({"filename"});
    options.positional_help("filename");
    auto result = parse_args(options, args);

    if(result.count("filename") < 1)
    {
        std::print("{}\n", options.help());
        return;
    }

    bool verbose = result.count("verbose") > 0;
    bool no_lang = result.count("no-lang") > 0;

    auto module_path = fs::absolute(fs::path{result["filename"].as<std::string>()});
    if(!module_path.has_extension())
    {
        module_path.replace_extension(package::module_ext);
    }

    // Add search paths.
    slang::file_manager file_mgr;
    file_mgr.add_search_path(module_path.parent_path());

    if(!no_lang)
    {
        if(verbose)
        {
            std::print("Info: Adding 'lang' to search paths.\n");
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
                std::print("Info: Adding '{}' to search paths.\n", it);
            }

            file_mgr.add_search_path(it);
        }
    }

    // Forwarded arguments.
    auto separator = std::ranges::find(std::as_const(args), "--");
    std::vector<std::string> forwarded_args{
      separator != args.cend()
        ? separator + 1
        : args.cend(),
      args.cend()};

    // Get module name.
    if(!file_mgr.is_file(module_path))
    {
        throw std::runtime_error(std::format(
          "Compiled module '{}' does not exist.",
          module_path.string()));
    }

    auto module_name = module_path.filename().stem();
    if(module_name.string().empty())
    {
        throw std::runtime_error(std::format(
          "Trying to get module name from path '{}' produced empty string.",
          module_path.string()));
    }

    if(verbose)
    {
        std::print("Info: module name: {}\n", module_name.string());
    }

    // Set up interpreter context.
    si::context ctx{file_mgr};
    runtime_setup(ctx, verbose);

    si::module_loader& loader = ctx.resolve_module(module_name);
    si::function& main_function = loader.get_function("main");
    validate_main_signature(main_function, verbose);

    // call 'main'.
    if(verbose)
    {
        std::print("Info: Invoking 'main'.\n");
    }
    si::value res = si::invoke(
      main_function,
      std::vector<std::string>{forwarded_args.begin(), forwarded_args.end()});

    const int* return_value = res.get<int>();

    if(return_value == nullptr)
    {
        std::print("\nProgram did not return a valid exit code.\n");
    }
    else
    {
        std::print("\nProgram exited with exit code {}.\n", *return_value);
    }

    check_finalized_gc(ctx.get_gc(), verbose);
}

std::string run::get_description() const
{
    return "Run a module.";
}

}    // namespace slang::commandline
