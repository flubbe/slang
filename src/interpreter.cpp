/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "interpreter.h"
#include "module.h"
#include "opcodes.h"

namespace slang::interpreter
{

call_result context::exec(const language_module& mod, std::size_t offset)
{
    const std::vector<std::byte>& binary = mod.get_binary();
    if(offset >= binary.size())
    {
        throw interpreter_error(fmt::format("context::exec: offset greater-equal to binary size ({} >= {}).", offset, binary.size()));
    }

    exec_stack stack;
    result_type last_load_type{result_type::rt_void};
    while(offset < binary.size())
    {
        auto instr = binary[offset];
        ++offset;

        if(instr >= static_cast<std::byte>(opcode::opcode_count))
        {
            throw interpreter_error(fmt::format("Invalid opcode '{}'.", static_cast<std::uint8_t>(instr)));
        }

        if(static_cast<opcode>(instr) == opcode::ret)
        {
            break;
        }
        last_load_type = result_type::rt_void;

        switch(static_cast<opcode>(instr))
        {
        case opcode::iload:
        {
            uint8_t i_u8[4] = {
              static_cast<std::uint8_t>(binary[offset]),
              static_cast<std::uint8_t>(binary[offset + 1]),
              static_cast<std::uint8_t>(binary[offset + 2]),
              static_cast<std::uint8_t>(binary[offset + 3])};
            offset += 4;

            stack.push_i32(i_u8[0] + (i_u8[1] << 8) + (i_u8[2] << 16) + (i_u8[3] << 24));
            last_load_type = result_type::rt_int;

            break;
        } /* opcode::iload */

        default:
            throw interpreter_error(fmt::format("Opcode '{}' not implemented.", to_string(static_cast<opcode>(instr))));
        }
    }

    if(last_load_type == result_type::rt_int)
    {
        return {stack.pop_i32()};
    }

    if(last_load_type != result_type::rt_void)
    {
        throw interpreter_error(fmt::format("Expected result type 0 (void), got '{}'", static_cast<int>(last_load_type)));
    }

    return {};
}

void context::load_module(const std::string& name, const language_module& mod)
{
    if(module_map.find(name) != module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' already loaded.", name));
    }

    module_map[name] = mod;

    // resolve dependencies.
    auto& header = mod.get_header();
    for(auto& it: header.imports)
    {
        throw interpreter_error("context::load_module: import resolution not implemented.");
    }
}

call_result context::invoke(const std::string& module_name, const std::string& function_name)
{
    auto mod_it = module_map.find(module_name);
    if(mod_it == module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' not loaded.", module_name));
    }

    auto& header = mod_it->second.get_header();
    for(auto& it: header.exports)
    {
        if(it.type != symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<function_descriptor>(it.desc);

        if(it.name == function_name)
        {
            if(desc.native)
            {
                throw interpreter_error("context::invoke not implemented for native functions.");
            }
            else
            {
                auto& details = std::get<function_details>(desc.details);
                return exec(mod_it->second, details.offset);
            }

            break;
        }
    }

    throw std::runtime_error("context::invoke: code path not implemented.");
}

}    // namespace slang::interpreter