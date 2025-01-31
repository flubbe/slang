/**
 * slang - a simple scripting language.
 *
 * commands to be executed from the command line.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "archives/file.h"
#include "codegen.h"
#include "commandline.h"
#include "compiler.h"
#include "emitter.h"
#include "interpreter/interpreter.h"
#include "module.h"
#include "parser.h"
#include "resolve.h"
#include "runtime/runtime.h"
#include "typing.h"
#include "utils.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace ty = slang::typing;
namespace rs = slang::resolve;
namespace rt = slang::runtime;
namespace si = slang::interpreter;

namespace slang::commandline
{

/**
 * Protected package names. Can only be removed
 * when --protected is specified on the command line.
 */
const std::array<std::string, 1> protected_names = {
  "std"};

/*
 * command implementation.
 */

void command::validate_name() const
{
    for(const auto& c: name)
    {
        if(c != '_' && !std::isalpha(c))
        {
            throw std::runtime_error(fmt::format("Invalid command name '{}'.", name));
        }
    }
}

/*
 * pkg implementation (package management).
 */

pkg::pkg(slang::package_manager& in_manager)
: command{"pkg"}
, manager{in_manager}
{
}

void pkg::invoke(const std::vector<std::string>& args)
{
    if(args.size() == 0)
    {
        const std::vector<std::pair<std::string, std::string>> cmd_help =
          {
            {"create pkg_name", "Create a new package."},
            {"info", "Output information about the current environment"},
            {"list [--all]", "Print a list of installed packages. If --all is specified, also lists sub-packages."},
            {"remove [--protected] pkg_name", "Remove a package."}};

        slang::utils::print_command_help("Usage: slang pkg command [args...]", cmd_help);
        return;
    }
    else
    {
        if(args[0] == "create")
        {
            if(args.size() != 2)
            {
                const std::string help_text =
                  "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
                  "is a valid package name.";
                slang::utils::print_usage_help("slang pkg create pkg_name", help_text);
                return;
            }

            const std::string package_name = args[1];
            fmt::print("Creating package '{}'...\n", package_name);

            if(!package::is_valid_name(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot create package '{}': The name is not a valid package name.", package_name));
            }

            if(manager.exists(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot create package '{}', since it already exists.", package_name));
            }

            slang::package pkg = manager.open(package_name, true);
            if(!pkg.is_persistent())
            {
                pkg.make_persistent();
            }

            fmt::print("Package '{}' successfully created.\n", package_name);
        }
        else if(args[0] == "info")
        {
            fmt::print("Package environment:\n\n");
            fmt::print("    Package root:            {}\n", manager.get_root_path().string());
            fmt::print("    Persistent package root: {}\n", manager.is_persistent());
            fmt::print("\n");
        }
        else if(args[0] == "list")
        {
            bool include_sub_packages = false;

            if(args.size() >= 2)
            {
                if(args.size() > 2 || args[1] != "--all")
                {
                    slang::utils::print_usage_help(
                      "slang pkg list [--all]",
                      "Print a list of installed packages. If --all is specified, also lists sub-packages.");
                    return;
                }
                include_sub_packages = true;
            }

            std::vector<std::string> package_names = manager.get_package_names(include_sub_packages);

            if(package_names.size() > 0)
            {
                fmt::print("Packages:\n\n");
                for(const auto& name: package_names)
                {
                    fmt::print("    {}\n", name);
                }
                fmt::print("\n");
            }
            else
            {
                fmt::print("No packages found.\n");
            }
        }
        else if(args[0] == "remove")
        {
            auto print_help = []()
            {
                const std::string help_text =
                  "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
                  "is a valid package name.";
                slang::utils::print_usage_help("slang pkg remove [--protected] pkg_name", help_text);
            };

            if(args.size() != 2 && args.size() != 3)
            {
                print_help();
                return;
            }

            std::string package_name;
            bool remove_protected = false;
            if(args.size() == 3)
            {
                if(args[1] != "--protected")
                {
                    print_help();
                    return;
                }
                remove_protected = true;
                package_name = args[2];
            }
            else
            {
                package_name = args[1];
            }

            // check if we're about to remove a protected package
            if(std::find(protected_names.begin(), protected_names.end(), package_name) != protected_names.end() && !remove_protected)
            {
                fmt::print("Cannot remove protected package '{}'. If you really want to remove it, specify --protected on the command line.\n", package_name);
                return;
            }

            fmt::print("Removing package '{}'...\n", package_name);

            if(!package::is_valid_name(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot remove package '{}': The name is not a valid package name.", package_name));
            }

            if(!manager.exists(package_name))
            {
                throw std::runtime_error(fmt::format("Cannot remove package '{}', since it does not exist.", package_name));
            }

            manager.remove(package_name);

            if(!manager.exists(package_name))
            {
                fmt::print("Package '{}' successfully removed.\n", package_name);
            }
            else
            {
                throw std::runtime_error(fmt::format("Could not remove package '{}'.", package_name));
            }
        }
        else
        {
            throw std::runtime_error(fmt::format("Unknown argument '{}'.", args[0]));
        }
    }
}

std::string pkg::get_description() const
{
    return "Manage packages.";
}

/*
 * Package builds.
 */

build::build(slang::package_manager& in_manager)
: command{"build"}
, manager{in_manager}
{
}

void build::invoke(const std::vector<std::string>& args)
{
    if(args.size() != 1)
    {
        const std::string help_text =
          "pkg_name consists of package identifiers, separated by ::. For example, std::utils "
          "is a valid package name.\n\nWhen a package is built, all of its sub-packages are also built.";
        slang::utils::print_usage_help("slang build pkg_name", help_text);
        return;
    }

    const std::string package_name = args[0];
    if(!package::is_valid_name(package_name))
    {
        throw std::runtime_error(fmt::format("Cannot build package '{}': The name is not a valid package name.", package_name));
    }

    if(!manager.exists(package_name))
    {
        throw std::runtime_error(fmt::format("Cannot build package '{}', since it does not exist.", package_name));
    }

    fmt::print("Building package '{}'...\n", package_name);

    // get all module sources.
    std::vector<package> pkgs{manager.open(package_name, false)};
    std::vector<std::string> sub_package_names = manager.get_package_names(true, package_name);

    fmt::print("Collected:\n");
    fmt::print("    * {}\n", package_name);
    for(auto& n: sub_package_names)
    {
        fmt::print("    * {}\n", n);
        pkgs.push_back(manager.open(n, false));
    }

    slang::compiler compiler{manager};

    // first pass.
    compiler.setup(pkgs);
    compiler.invoke_lexer();
    compiler.collect_definitions();
    pkgs = compiler.resolve_dependencies();

    // second pass.
    try
    {
        for(auto& p: pkgs)
        {
            compiler.compile_package(p);
        }
    }
    catch(const slang::compiler_error& e)
    {
        fmt::print("{}\n", e.what());
        fmt::print("Compilation failed.\n");
        return;
    }
}

std::string build::get_description() const
{
    return "Build a package.";
}

/*
 * Compile single module.
 */

compile::compile(slang::package_manager& in_manager)
: command{"compile"}
, manager{in_manager}
{
}

void compile::invoke(const std::vector<std::string>& args)
{
    bool display_help_and_exit = false;
    fs::path module_path;
    fs::path output_file;

    if(args.size() < 1)
    {
        display_help_and_exit = true;
    }

    std::vector<std::string>::const_iterator output_file_it;
    if(!display_help_and_exit)
    {
        // get input file.
        module_path = fs::path(args[0]);
        if(!module_path.has_extension())
        {
            module_path.replace_extension(package::source_ext);
        }

        // get output file.
        output_file_it = std::find(args.begin(), args.end(), "-o");
        if(output_file_it != args.end())
        {
            if(output_file_it + 1 == args.end())
            {
                fmt::print(" -o <output-file>: Missing <output-file>.\n");
                return;
            }

            output_file = *(output_file_it + 1);
            if(!output_file.has_extension())
            {
                output_file.replace_extension(package::module_ext);
            }
        }
        else
        {
            output_file = module_path;
            output_file.replace_extension(package::module_ext);
        }
    }

    if(display_help_and_exit)
    {
        const std::string help_text =
          "Compile the module `mod.sl` into `mod.cmod`.";
        slang::utils::print_usage_help("slang compile mod", help_text);
        return;
    }

    auto evaluate_constant_subexpressions_it =
      std::find(
        args.begin(),
        args.end(),
        "--no-eval-const-subexpr");
    bool evaluate_constant_subexpressions =
      (evaluate_constant_subexpressions_it == args.end())
      || (evaluate_constant_subexpressions_it == output_file_it);

    auto verbose_it =
      std::find(
        args.begin(),
        args.end(),
        "--verbose");
    bool verbose = (verbose_it != args.end()) && (verbose_it != output_file_it);

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

    slang::file_manager file_mgr;
    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");

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
        input_ar->serialize(reinterpret_cast<std::byte*>(&input_buffer[0]), input_size);
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

    ty::context type_ctx;
    rs::context resolve_ctx{file_mgr};
    cg::context codegen_ctx;
    slang::instruction_emitter emitter{codegen_ctx};

    codegen_ctx.evaluate_constant_subexpressions = evaluate_constant_subexpressions;

    ast->collect_names(codegen_ctx, type_ctx);
    resolve_ctx.resolve_imports(codegen_ctx, type_ctx);
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

/*
 * Package execution.
 */

class instruction_logger : public slang::interpreter::instruction_recorder
{
    /** Constant table entries. */
    std::size_t constant_entries{0};

    /** Import table entry count. */
    std::size_t import_entries{0};

    /** Export table entry count */
    std::size_t export_entries{0};

public:
    virtual void section(const std::string& name) override
    {
        fmt::print("--- {} ---\n", name);
    }

    virtual void function(const std::string& name, const module_::function_details& details) override
    {
        fmt::print(
          "{:>4}: @{} (size {}, args {}, locals {})\n",
          details.offset, name, details.size, details.args_size, details.locals_size);
    }

    virtual void type(const std::string& name, const module_::struct_descriptor& desc) override
    {
        fmt::print("%{} = type (size {}, alignment {}, flags {}) {{\n", name, desc.size, desc.alignment, desc.flags);

        for(std::size_t i = 0; i < desc.member_types.size(); ++i)
        {
            auto& [member_name, member_type] = desc.member_types[i];
            fmt::print("    {} %{} (offset {}, size {}, alignment {}){}\n",
                       to_string(member_type.base_type),
                       member_name,
                       member_type.offset,
                       member_type.size,
                       member_type.alignment,
                       i != desc.member_types.size() - 1
                         ? ","
                         : "");
        }

        fmt::print("}}\n");
    }

    virtual void constant(const module_::constant_table_entry& c) override
    {
        fmt::print("{:>3}: {:>3}, ", constant_entries, to_string(c.type));
        if(c.type == module_::constant_type::i32)
        {
            fmt::print("{}\n", std::get<std::int32_t>(c.data));
        }
        else if(c.type == module_::constant_type::f32)
        {
            fmt::print("{}\n", std::get<float>(c.data));
        }
        else if(c.type == module_::constant_type::str)
        {
            fmt::print("{}\n", std::get<std::string>(c.data));
        }
        ++constant_entries;
    }

    virtual void record(const module_::exported_symbol& s) override
    {
        fmt::print("{:>3}: {:>11}, {}", export_entries, to_string(s.type), s.name);
        if(s.type == module_::symbol_type::constant)
        {
            fmt::print(", {}", std::get<std::size_t>(s.desc));
        }
        fmt::print("\n");
        ++export_entries;
    }

    virtual void record(const module_::imported_symbol& s) override
    {
        fmt::print("{:>3}: {:>11}, {}, {}\n", import_entries, to_string(s.type), s.name, static_cast<std::int32_t>(s.package_index));
        ++import_entries;
    }

    virtual void label(std::int64_t index) override
    {
        fmt::print("%{}:\n", index);
    }

    virtual void record(opcode instr) override
    {
        fmt::print("    {:>11}\n", to_string(instr));
    }

    virtual void record(opcode instr, std::int64_t i) override
    {
        fmt::print("    {:>11}    {}\n", to_string(instr), i);
    }

    virtual void record(opcode instr, std::int64_t i1, std::int64_t i2) override
    {
        fmt::print("    {:>11}    {}, {}\n", to_string(instr), i1, i2);
    }

    virtual void record(opcode instr, float f) override
    {
        fmt::print("    {:>11}    {}\n", to_string(instr), f);
    }

    virtual void record(opcode instr, std::int64_t i, std::string s) override
    {
        fmt::print("    {:>11}    {} ({})\n", to_string(instr), i, s);
    }

    virtual void record(
      opcode instr,
      std::int64_t i,
      std::string s,
      std::int64_t field_index) override
    {
        fmt::print("    {:>11}    {} ({}), {}\n", to_string(instr), i, s, field_index);
    }

    virtual void record(opcode instr, std::string s1, std::string s2) override
    {
        fmt::print("    {:>11}    {}, {}\n", to_string(instr), s1, s2);
    }

    virtual void record(opcode instr, std::string s1, std::string s2, std::string s3) override
    {
        fmt::print("    {:>11}    {}, {}, {}\n", to_string(instr), s1, s2, s3);
    }
};

exec::exec(slang::package_manager& in_manager)
: command{"exec"}
, manager{in_manager}
{
}

void exec::invoke(const std::vector<std::string>& args)
{
    if(args.size() < 1)
    {
        const std::string help_text =
          "Execute the main function of the module `mod.cmod`.";
        slang::utils::print_usage_help("slang exec mod", help_text);
        return;
    }

    auto separator = std::find(args.begin(), args.end(), "--");
    std::vector<std::string> forwarded_args{
      separator != args.end()
        ? separator + 1
        : args.end(),
      args.end()};

    bool disassemble = std::find(args.begin(), separator, "--disasm") != separator;

    auto module_path = fs::path(args[0]);
    if(!module_path.has_extension())
    {
        module_path.replace_extension(package::module_ext);
    }

    slang::file_manager file_mgr;
    file_mgr.add_search_path(module_path.parent_path());

    file_mgr.add_search_path(".");
    file_mgr.add_search_path("lang");

    if(!file_mgr.is_file(module_path))
    {
        throw std::runtime_error(fmt::format(
          "Compiled module '{}' does not exist.",
          module_path.string()));
    }

    auto module_name = module_path.filename().stem();
    if(module_name.string().length() == 0)
    {
        throw std::runtime_error(fmt::format(
          "Trying to get module name from path '{}' produced empty string.",
          module_path.string()));
    }

    si::context ctx{file_mgr};

    rt::register_builtin_type_layouts(ctx.get_gc());

    ctx.register_native_function("slang", "print",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     fmt::print("{}", *s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     std::string* s = stack.pop_addr<std::string>();
                                     fmt::print("{}\n", *s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "array_copy",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::array_copy(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_length",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_length(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_equals",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_equals(ctx, stack);
                                 });
    ctx.register_native_function("slang", "string_concat",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::string_concat(ctx, stack);
                                 });
    ctx.register_native_function("slang", "i32_to_string",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::i32_to_string(ctx, stack);
                                 });
    ctx.register_native_function("slang", "f32_to_string",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::f32_to_string(ctx, stack);
                                 });
    ctx.register_native_function("slang", "parse_i32",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::parse_i32(ctx, stack);
                                 });
    ctx.register_native_function("slang", "parse_f32",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::parse_f32(ctx, stack);
                                 });
    ctx.register_native_function("slang", "assert",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::assert_(ctx, stack);
                                 });

    /*
     * Math.
     */

    ctx.register_native_function("slang", "abs",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::abs(ctx, stack);
                                 });
    ctx.register_native_function("slang", "sqrt",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::sqrt(ctx, stack);
                                 });
    ctx.register_native_function("slang", "ceil",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::ceil(ctx, stack);
                                 });
    ctx.register_native_function("slang", "floor",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::floor(ctx, stack);
                                 });
    ctx.register_native_function("slang", "trunc",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::trunc(ctx, stack);
                                 });
    ctx.register_native_function("slang", "round",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::round(ctx, stack);
                                 });
    ctx.register_native_function("slang", "sin",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::sin(ctx, stack);
                                 });
    ctx.register_native_function("slang", "cos",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::cos(ctx, stack);
                                 });
    ctx.register_native_function("slang", "tan",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::tan(ctx, stack);
                                 });
    ctx.register_native_function("slang", "asin",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::asin(ctx, stack);
                                 });
    ctx.register_native_function("slang", "acos",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::acos(ctx, stack);
                                 });
    ctx.register_native_function("slang", "atan",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::atan(ctx, stack);
                                 });
    ctx.register_native_function("slang", "atan2",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     rt::atan2(ctx, stack);
                                 });

    if(disassemble)
    {
        auto recorder = std::make_shared<instruction_logger>();
        ctx.resolve_module(module_name, recorder);
        return;
    }

    si::module_loader& loader = ctx.resolve_module(module_name);
    si::function& main_function = loader.get_function("main");

    // validate function signature for 'main'.
    const module_::function_signature& sig = main_function.get_signature();
    if(sig.return_type.base_type() != "i32" || sig.return_type.is_array())
    {
        throw std::runtime_error(fmt::format(
          "Invalid return type for 'main' expected 'i32', got '{}'.",
          slang::module_::to_string(sig.return_type)));
    }
    if(sig.arg_types.size() != 1)
    {
        throw std::runtime_error(fmt::format(
          "Invalid parameter count for 'main'. Expected 1 parameter, got {}.",
          sig.arg_types.size()));
    }
    if(sig.arg_types[0].base_type() != "str" || !sig.arg_types[0].is_array())
    {
        throw std::runtime_error(fmt::format(
          "Invalid parameter type for 'main'. Expected parameter of type '[str]', got '{}'.",
          slang::module_::to_string(sig.arg_types[0])));
    }

    // call 'main'.
    si::value res = si::invoke(
      ctx,
      loader,
      main_function,
      std::vector<std::string>{forwarded_args.begin(), forwarded_args.end()});

    const int* return_value = res.get<int>();

    if(return_value == nullptr)
    {
        fmt::print("\nProgram did not return a valid exit code.\n");
    }
    else
    {
        fmt::print("\nProgram exited with exit code {}.\n", *return_value);
    }

    if(ctx.get_gc().object_count() != 0)
    {
        fmt::print("GC warning: Object count is {}.\n", ctx.get_gc().object_count());
    }
    if(ctx.get_gc().root_set_size() != 0)
    {
        fmt::print("GC warning: Root set size is {}.\n", ctx.get_gc().root_set_size());
    }
    if(ctx.get_gc().byte_size() != 0)
    {
        fmt::print("GC warning: {} bytes still allocated.\n", ctx.get_gc().byte_size());
    }
}

std::string exec::get_description() const
{
    return "Execute a module.";
}

}    // namespace slang::commandline