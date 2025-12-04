/**
 * slang - a simple scripting language.
 *
 * `disasm` command implementation.
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
        std::println("--- {} ---", name);
    }

    void function(const std::string& name, const module_::function_details& details) override
    {
        std::println(
          "{:>4}: @{} (size {}, args {}, locals {})",
          details.offset, name, details.size, details.args_size, details.locals_size);
    }

    void type(const std::string& name, const module_::struct_descriptor& desc) override
    {
        std::println("%{} = type (size {}, alignment {}, flags {}) {{", name, desc.size, desc.alignment, desc.flags);

        for(std::size_t i = 0; i < desc.member_types.size(); ++i)
        {
            const auto& [member_name, member_type] = desc.member_types[i];
            std::println(
              "    {} %{} (offset {}, size {}, alignment {}){}",
              to_string(member_type.base_type),
              member_name,
              member_type.offset,
              member_type.size,
              member_type.alignment,
              i != desc.member_types.size() - 1
                ? ","
                : "");
        }

        std::println("}}");
    }

    void constant(const module_::constant_table_entry& c) override
    {
        std::print("{:>3}: {:>3}, ", constant_entries, to_string(c.type));
        if(c.type == module_::constant_type::i32)
        {
            std::println("{}", std::get<std::int32_t>(c.data));
        }
        else if(c.type == module_::constant_type::i64)
        {
            std::println("{}", std::get<std::int64_t>(c.data));
        }
        else if(c.type == module_::constant_type::f32)
        {
            std::println("{}", std::get<float>(c.data));
        }
        else if(c.type == module_::constant_type::f64)
        {
            std::println("{}", std::get<double>(c.data));
        }
        else if(c.type == module_::constant_type::str)
        {
            std::println("{}", std::get<std::string>(c.data));
        }
        ++constant_entries;
    }

    void record(const module_::exported_symbol& s) override
    {
        std::print("{:>3}: {:>11}, {}", export_entries, to_string(s.type), s.name);
        if(s.type == module_::symbol_type::constant)
        {
            std::print(", {}", std::get<std::size_t>(s.desc));
        }
        std::println();
        ++export_entries;
    }

    void record(const module_::imported_symbol& s) override
    {
        std::println("{:>3}: {:>11}, {}, {}", import_entries, to_string(s.type), s.name, static_cast<std::int32_t>(s.package_index));
        ++import_entries;
    }

    void label(std::int64_t index) override
    {
        std::println("%{}:", index);
    }

    void record(opcode instr) override
    {
        std::println("    {:>11}", to_string(instr));
    }

    void record(opcode instr, std::int64_t i) override
    {
        std::println("    {:>11}    {}", to_string(instr), i);
    }

    void record(opcode instr, std::int64_t i1, std::int64_t i2) override
    {
        std::println("    {:>11}    {}, {}", to_string(instr), i1, i2);
    }

    void record(opcode instr, float f) override
    {
        std::println("    {:>11}    {}", to_string(instr), f);
    }

    void record(opcode instr, double d) override
    {
        std::println("    {:>11}    {}", to_string(instr), d);
    }

    void record(opcode instr, std::int64_t i, std::string s) override
    {
        std::println("    {:>11}    {} ({})", to_string(instr), i, s);
    }

    void record(
      opcode instr,
      std::int64_t i,
      std::string s,
      std::int64_t field_index) override
    {
        std::println("    {:>11}    {} ({}), {}", to_string(instr), i, s, field_index);
    }

    void record(opcode instr, std::string s1, std::string s2) override
    {
        std::println("    {:>11}    {}, {}", to_string(instr), s1, s2);
    }

    void record(opcode instr, std::string s1, std::string s2, std::string s3) override
    {
        std::println("    {:>11}    {}, {}, {}", to_string(instr), s1, s2, s3);
    }
};

/*
 * disasm implementation.
 */

disasm::disasm(slang::package_manager& manager)
: command{"disasm"}
, manager{manager}
{
}

void disasm::invoke(const std::vector<std::string>& args)
{
    cxxopts::Options options = make_cxxopts_options();

    // clang-format off
    options.add_options()
        ("no-lang", "Exclude default language modules.")
        ("filename", "The file to disassemble.", cxxopts::value<std::string>());
    // clang-format on

    options.parse_positional({"filename"});
    options.positional_help("filename");
    auto result = parse_args(options, args);

    if(result.count("filename") < 1)
    {
        std::println("{}", options.help());
        return;
    }

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
        file_mgr.add_search_path("lang");
    }

    if(result.count("search-path") > 0)
    {
        std::vector<std::string> v = utils::split(result["search-path"].as<std::string>(), ";");
        for(auto& it: v)
        {
            file_mgr.add_search_path(it);
        }
    }

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

    std::print("Module name: {}\n\n", module_name.string());

    // Set up interpreter context. The disassembly happens during module resolution.
    si::context ctx{file_mgr};
    runtime_setup(ctx);
    auto recorder = std::make_shared<instruction_logger>();
    ctx.resolve_module(module_name, recorder);
}

std::string disasm::get_description() const
{
    return "Disassemble a module.";
}

}    // namespace slang::commandline
