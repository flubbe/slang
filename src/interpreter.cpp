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

#include "archives/memory.h"
#include "interpreter.h"
#include "module.h"
#include "opcodes.h"

namespace slang::interpreter
{

value context::exec(const language_module& mod, const function& f, const std::vector<value>& args)
{
    if(args.size() != 0)
    {
        throw std::runtime_error("context::exec: arguments not implemented.");
    }

    const std::vector<std::byte>& binary = mod.get_binary();
    std::size_t offset = f.get_entry_point();
    if(offset >= binary.size())
    {
        throw interpreter_error(fmt::format("Entry point is outside the loaded code segment ({} >= {}).", offset, binary.size()));
    }

    exec_stack stack;
    while(offset < binary.size())
    {
        auto instr = binary[offset];
        ++offset;

        if(static_cast<opcode>(instr) == opcode::ret)
        {
            break;
        }

        switch(static_cast<opcode>(instr))
        {
        case opcode::iadd:
        {
            stack.push_i32(stack.pop_i32() + stack.pop_i32());
            break;
        } /* opcode::iadd */
        case opcode::isub:
        {
            stack.push_i32(-stack.pop_i32() + stack.pop_i32());
            break;
        } /* opcode::isub */
        case opcode::imul:
        {
            stack.push_i32(stack.pop_i32() * stack.pop_i32());
            break;
        } /* opcode::imul */
        case opcode::idiv:
        {
            std::int32_t divisor = stack.pop_i32();
            if(divisor == 0)
            {
                throw interpreter_error("Division by zero.");
            }
            std::int32_t dividend = stack.pop_i32();
            stack.push_i32(dividend / divisor);
            break;
        } /* opcode::idiv */
        case opcode::fadd:
        {
            stack.push_f32(stack.pop_f32() + stack.pop_f32());
            break;
        } /* opcode::fadd */
        case opcode::fsub:
        {
            stack.push_f32(-stack.pop_f32() + stack.pop_f32());
            break;
        } /* opcode::fsub */
        case opcode::fmul:
        {
            stack.push_f32(stack.pop_f32() * stack.pop_f32());
            break;
        } /* opcode::fmul */
        case opcode::fdiv:
        {
            float divisor = stack.pop_f32();
            if(divisor == 0)
            {
                throw interpreter_error("Division by zero.");
            }
            float dividend = stack.pop_f32();
            stack.push_f32(dividend / divisor);
            break;
        } /* opcode::fdiv */
        case opcode::iconst:
        {
            if(offset + 4 >= binary.size())
            {
                throw interpreter_error("Instruction tried to read outside of code segment.");
            }

            uint8_t i_u8[4] = {
              static_cast<std::uint8_t>(binary[offset]),
              static_cast<std::uint8_t>(binary[offset + 1]),
              static_cast<std::uint8_t>(binary[offset + 2]),
              static_cast<std::uint8_t>(binary[offset + 3])};
            offset += 4;

            stack.push_i32(i_u8[0] + (i_u8[1] << 8) + (i_u8[2] << 16) + (i_u8[3] << 24));

            break;
        } /* opcode::iconst */
        case opcode::fconst:
        {
            if(offset + 4 >= binary.size())
            {
                throw interpreter_error("Instruction tried to read outside of code segment.");
            }

            uint8_t f_u8[4] = {
              static_cast<std::uint8_t>(binary[offset]),
              static_cast<std::uint8_t>(binary[offset + 1]),
              static_cast<std::uint8_t>(binary[offset + 2]),
              static_cast<std::uint8_t>(binary[offset + 3])};
            offset += 4;

            // push i32, will be read as f32.
            stack.push_i32(f_u8[0] + (f_u8[1] << 8) + (f_u8[2] << 16) + (f_u8[3] << 24));

            break;
        } /* opcode::fconst */

        default:
            throw interpreter_error(fmt::format("Opcode '{}' not implemented.", to_string(static_cast<opcode>(instr))));
        }
    }

    auto& signature = f.get_signature();
    if(signature.return_type == "i32")
    {
        return {stack.pop_i32()};
    }
    else if(signature.return_type == "f32")
    {
        return {stack.pop_f32()};
    }

    if(signature.return_type != "void")
    {
        throw interpreter_error(fmt::format("Expected result type 'void', got '{}'", signature.return_type));
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

    // populate function map.
    std::unordered_map<std::string, function> fmap;
    for(auto& it: header.exports)
    {
        if(it.type != symbol_type::function)
        {
            continue;
        }

        if(fmap.find(it.name) != fmap.end())
        {
            throw interpreter_error(fmt::format("Function '{}' already exists in export map.", it.name));
        }

        auto& desc = std::get<function_descriptor>(it.desc);
        if(desc.native)
        {
            throw interpreter_error("context::load_module: not implemented for native functions.");
        }
        else
        {
            fmap.insert({it.name, function(desc.signature, std::get<function_details>(desc.details).offset)});
        }
    }
    function_map.insert({name, std::move(fmap)});
}

value context::invoke(const std::string& module_name, const std::string& function_name, const std::vector<value>& args)
{
    auto mod_it = module_map.find(module_name);
    if(mod_it == module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' not found.", module_name));
    }

    auto func_mod_it = function_map.find(module_name);
    if(func_mod_it == function_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' not found.", module_name));
    }

    auto func_it = func_mod_it->second.find(function_name);
    if(func_it == func_mod_it->second.end())
    {
        throw interpreter_error(fmt::format("Function '{}' not found in module '{}'.", function_name, module_name));
    }

    return exec(mod_it->second, func_it->second, args);
}

}    // namespace slang::interpreter