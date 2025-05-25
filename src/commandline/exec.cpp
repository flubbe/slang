/**
 * slang - a simple scripting language.
 *
 * `exec` command implementation.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

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

/** Instruction logger for disassembly. */
class instruction_logger : public slang::interpreter::instruction_recorder
{
    /** Constant table entries. */
    std::size_t constant_entries{0};

    /** Import table entry count. */
    std::size_t import_entries{0};

    /** Export table entry count */
    std::size_t export_entries{0};

public:
    void section(const std::string& name) override
    {
        fmt::print("--- {} ---\n", name);
    }

    void function(const std::string& name, const module_::function_details& details) override
    {
        fmt::print(
          "{:>4}: @{} (size {}, args {}, locals {})\n",
          details.offset, name, details.size, details.args_size, details.locals_size);
    }

    void type(const std::string& name, const module_::struct_descriptor& desc) override
    {
        fmt::print("%{} = type (size {}, alignment {}, flags {}) {{\n", name, desc.size, desc.alignment, desc.flags);

        for(std::size_t i = 0; i < desc.member_types.size(); ++i)
        {
            const auto& [member_name, member_type] = desc.member_types[i];
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

    void constant(const module_::constant_table_entry& c) override
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

    void record(const module_::exported_symbol& s) override
    {
        fmt::print("{:>3}: {:>11}, {}", export_entries, to_string(s.type), s.name);
        if(s.type == module_::symbol_type::constant)
        {
            fmt::print(", {}", std::get<std::size_t>(s.desc));
        }
        fmt::print("\n");
        ++export_entries;
    }

    void record(const module_::imported_symbol& s) override
    {
        fmt::print("{:>3}: {:>11}, {}, {}\n", import_entries, to_string(s.type), s.name, static_cast<std::int32_t>(s.package_index));
        ++import_entries;
    }

    void label(std::int64_t index) override
    {
        fmt::print("%{}:\n", index);
    }

    void record(opcode instr) override
    {
        fmt::print("    {:>11}\n", to_string(instr));
    }

    void record(opcode instr, std::int64_t i) override
    {
        fmt::print("    {:>11}    {}\n", to_string(instr), i);
    }

    void record(opcode instr, std::int64_t i1, std::int64_t i2) override
    {
        fmt::print("    {:>11}    {}, {}\n", to_string(instr), i1, i2);
    }

    void record(opcode instr, float f) override
    {
        fmt::print("    {:>11}    {}\n", to_string(instr), f);
    }

    void record(opcode instr, std::int64_t i, std::string s) override
    {
        fmt::print("    {:>11}    {} ({})\n", to_string(instr), i, s);
    }

    void record(
      opcode instr,
      std::int64_t i,
      std::string s,
      std::int64_t field_index) override
    {
        fmt::print("    {:>11}    {} ({}), {}\n", to_string(instr), i, s, field_index);
    }

    void record(opcode instr, std::string s1, std::string s2) override
    {
        fmt::print("    {:>11}    {}, {}\n", to_string(instr), s1, s2);
    }

    void record(opcode instr, std::string s1, std::string s2, std::string s3) override
    {
        fmt::print("    {:>11}    {}, {}, {}\n", to_string(instr), s1, s2, s3);
    }
};

/*
 * exec implementation.
 */

/**
 * Set up the default runtime environment for a context.
 *
 * @param ctx The context to set up.
 * @param verbose Whether to enable verbose logging.
 */
static void runtime_setup(si::context& ctx, bool verbose)
{
    if(verbose)
    {
        fmt::print("Info: Registering type layouts.\n");
    }

    rt::register_builtin_type_layouts(ctx.get_gc());

    if(verbose)
    {
        fmt::print("Info: Registering native functions.\n");
    }

    ctx.register_native_function("slang", "print",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
                                     fmt::print("{}", *s);
                                     ctx.get_gc().remove_temporary(s);
                                 });
    ctx.register_native_function("slang", "println",
                                 [&ctx](si::operand_stack& stack)
                                 {
                                     auto* s = stack.pop_addr<std::string>();
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
}

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
        fmt::print("Info: Validating signature of 'main'.\n");
    }

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
        fmt::print("Info: Checking GC cleanup.\n");
    }

    if(gc.object_count() != 0)
    {
        fmt::print("GC warning: Object count is {}.\n", gc.object_count());
    }
    if(gc.root_set_size() != 0)
    {
        fmt::print("GC warning: Root set size is {}.\n", gc.root_set_size());
    }
    if(gc.byte_size() != 0)
    {
        fmt::print("GC warning: {} bytes still allocated.\n", gc.byte_size());
    }
}

exec::exec(slang::package_manager& manager)
: command{"exec"}
, manager{manager}
{
}

void exec::invoke(const std::vector<std::string>& args)
{
    cxxopts::Options options = make_cxxopts_options();

    // clang-format off
    options.add_options()
        ("v,verbose", "Verbose output.")
        ("d,disasm", "Show disassembly and exit.")
        ("no-lang", "Exclude default language modules.")
        ("search-path", "Additional search paths for module resolution, separated by ';'.", cxxopts::value<std::string>())
        ("filename", "The file to compile", cxxopts::value<std::string>());
    // clang-format on

    options.parse_positional({"filename"});
    options.positional_help("filename");
    auto result = parse_args(options, args);

    if(result.count("filename") < 1)
    {
        fmt::print("{}\n", options.help());
        return;
    }

    bool verbose = result.count("verbose") > 0;
    bool disassemble = result.count("disasm") > 0;
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

    // Forwarded arguments.
    auto separator = std::find(args.begin(), args.end(), "--");
    std::vector<std::string> forwarded_args{
      separator != args.end()
        ? separator + 1
        : args.end(),
      args.end()};

    // Get module name.
    if(!file_mgr.is_file(module_path))
    {
        throw std::runtime_error(fmt::format(
          "Compiled module '{}' does not exist.",
          module_path.string()));
    }

    auto module_name = module_path.filename().stem();
    if(module_name.string().empty())
    {
        throw std::runtime_error(fmt::format(
          "Trying to get module name from path '{}' produced empty string.",
          module_path.string()));
    }

    if(verbose)
    {
        fmt::print("Info: module name: {}\n", module_name.string());
    }

    // Set up interpreter context.
    si::context ctx{file_mgr};
    runtime_setup(ctx, verbose);

    if(disassemble)
    {
        auto recorder = std::make_shared<instruction_logger>();
        ctx.resolve_module(module_name, recorder);
        return;
    }

    si::module_loader& loader = ctx.resolve_module(module_name);
    si::function& main_function = loader.get_function("main");
    validate_main_signature(main_function, verbose);

    // call 'main'.
    if(verbose)
    {
        fmt::print("Info: Invoking 'main'.\n");
    }
    si::value res = si::invoke(
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

    check_finalized_gc(ctx.get_gc(), verbose);
}

std::string exec::get_description() const
{
    return "Execute a module.";
}

}    // namespace slang::commandline
