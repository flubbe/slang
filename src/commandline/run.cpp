/**
 * slang - a simple scripting language.
 *
 * `run` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <chrono>
#include <format>
#include <print>

#include "archives/file.h"
#include "interpreter/interpreter.h"
#include "interpreter/invoke.h"

#ifdef SLANG_ARCH_AARCH64
#    include "jit/aarch64.h"
#endif /* SLANG_ARCH_AARCH64 */

#include "runtime/runtime.h"
#include "shared/module.h"
#include "commandline.h"
#include "utils.h"

namespace rt = slang::runtime;
namespace si = slang::interpreter;

#if SLANG_JIT_AVAILABLE
namespace sj = slang::jit;
#endif /* SLANG_JIT_AVAILABLE */

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
        std::println("Info: Validating signature of 'main'.");
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
          "Invalid parameter type for 'main'. Expected parameter of type 'str[]', got '{}'.",
          slang::module_::to_string(sig.arg_types[0])));
    }
}

/**
 * Finalize garbage collection.
 *
 * @param gc The garbage collector.
 * @param verbose Whether to enable verbose logging.
 */
static void finalize_gc(gc::garbage_collector& gc, bool verbose)
{
    if(verbose)
    {
        std::println("Info: Finalizing GC.");
    }

    gc.run();

    if(gc.object_count() != 0)
    {
        std::println("GC warning: Object count is {}.", gc.object_count());
    }
    if(gc.root_set_size() != 0)
    {
        std::println("GC warning: Root set size is {}.", gc.root_set_size());
    }
    if(gc.byte_size() != 0)
    {
        std::println("GC warning: {} bytes still allocated.", gc.byte_size());
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
#if SLANG_JIT_AVAILABLE
        ("j,jit", "Run using JIT compiler.")
#endif /* SLANG_JIT_AVAILABLE */
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
        std::println("{}", options.help());
        return;
    }

    bool verbose = result.count("verbose") > 0;
#if SLANG_JIT_AVAILABLE
    bool use_jit = result.count("jit") > 0;
#endif /* SLANG_JIT_AVAILABLE */
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
        std::println("Info: module name: {}", module_name.string());
    }

    /*
     * Set up interpreter context.
     *
     * TODO In the JIT path, this is used for bookkepping and should be
     *      factored into a common shared base used by interpreter and
     *      compiler.
     */
    si::context ctx{file_mgr};
    runtime_setup(ctx, verbose);

#if SLANG_JIT_AVAILABLE
    if(!use_jit)
    {
#endif /* SLANG_JIT_AVAILABLE */
        si::module_loader& loader = ctx.resolve_module(module_name);
        si::function& main_function = loader.get_function("main");
        validate_main_signature(main_function, verbose);

        // call 'main'.
        if(verbose)
        {
            std::println("Info: Invoking 'main'.");
        }
        si::value res = si::invoke(
          main_function,
          std::vector<std::string>{forwarded_args.begin(), forwarded_args.end()});

        const int* return_value = res.get<int>();

        if(return_value == nullptr)
        {
            std::println();
            std::println("Program did not return a valid exit code.");
        }
        else
        {
            std::println();
            std::println("Program exited with exit code {}.", *return_value);
        }

        finalize_gc(ctx.get_gc(), verbose);
#if SLANG_JIT_AVAILABLE
    }
    else
    {
        auto time_load_compile_start = std::chrono::steady_clock::now();

        sj::module_loader mod{
          file_mgr,
          module_name,
          fs::path{module_name}.replace_extension(package::module_ext),
          &ctx};

        mod.register_native_functions(
          [&ctx](const std::string& function_name, const std::string& library_name)
          {
              return ctx.resolve_native_function(function_name, library_name);
          });

        auto time_load_compile_end = std::chrono::steady_clock::now();

        if(verbose)
        {
            float load_compile_duration =
              std::chrono::duration<float, std::milli>(
                time_load_compile_end - time_load_compile_start)
                .count();
            std::println(
              "Info: Loaded and compiled modules in {:.2f} ms.",
              load_compile_duration);
        }

        auto& function = mod.get_function("main");

        // TODO Signature validation.

        // Check if there is space for the return value.
        if(function.get_stack_size() < sizeof(std::int32_t))
        {
            throw jit::jit_error{
              "Invalid return type size for 'main'."};
        }

        // Construct the arguments,
        auto* main_args = ctx.get_gc().gc_new_array<std::string*>(
          forwarded_args.size(),
          gc::gc_object::of_temporary);
        for(std::size_t i = 0; i < forwarded_args.size(); ++i)
        {
            auto* argument = ctx.get_gc().gc_new<std::string>(
              gc::gc_object::of_none,
              false);
            *argument = forwarded_args[i];
            (*main_args)[i] = argument;
        }

        // Construct the stack frame.
        auto frame = si::stack_frame::with_size(
          mod.get_constant_table(),
          function.get_locals_size(),
          function.get_stack_size());
        frame.gc = &ctx.get_gc();

        // Check that we have enough space available for the argument pointer.
        if(frame.locals.size() < sizeof(main_args))    // NOLINT(bugprone-sizeof-expression)
        {
            throw sj::jit_error{
              "JIT main function has no str[] argument slot."};
        }
        std::memcpy(
          frame.locals.data(),
          reinterpret_cast<const void*>(&main_args),    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          sizeof(main_args));                           // NOLINT(bugprone-sizeof-expression)
        frame.update_gc_local_root(0, main_args);

        function(&frame);

        // If there was an error, we re-throw the exception here
        // so it's visible to the caller/main.
        if(frame.pending_exception != nullptr)
        {
            std::rethrow_exception(frame.pending_exception);
        }

        std::int32_t return_value = 0;
        std::memcpy(
          &return_value,
          frame.stack.span().data(),
          sizeof(return_value));

        std::println();
        std::println("Program exited with exit code {}.", return_value);

        // Finalize GC.
        frame.clear_gc_local_roots();
        ctx.get_gc().remove_temporary(main_args);
        finalize_gc(ctx.get_gc(), verbose);
    }
#endif /* SLANG_JIT_AVAILABLE */
}

std::string run::get_description() const
{
    return "Run a module.";
}

}    // namespace slang::commandline
