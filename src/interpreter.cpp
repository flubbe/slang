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

std::pair<module_header, std::vector<std::byte>> context::decode(const language_module& mod) const
{
    module_header header = mod.get_header();
    const std::vector<std::byte>& binary = mod.get_binary();

    std::vector<std::byte> code;
    for(auto& it: header.exports)
    {
        if(it.type != symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<function_descriptor>(it.desc);
        if(desc.native)
        {
            continue;
        }

        auto& details = std::get<function_details>(desc.details);
        std::size_t decoded_offset = code.size();
        std::size_t end_offset = details.offset + details.size;
        for(std::size_t i = details.offset; i < end_offset;)
        {
            auto instr = binary[i];
            code.push_back(instr);
            ++i;

            switch(static_cast<opcode>(instr))
            {
            /* opcodes without arguments. */
            case opcode::ret:
            case opcode::iadd:
            case opcode::isub:
            case opcode::imul:
            case opcode::idiv:
            case opcode::fadd:
            case opcode::fsub:
            case opcode::fmul:
            case opcode::fdiv:
                break;
            /* opcodes with one 4-byte argument. */
            case opcode::iconst:
            case opcode::fconst:
            {
                if(i + 4 >= binary.size())
                {
                    throw interpreter_error("Cannot decode instruction (unexpected EOF).");
                }

                std::uint8_t i_u8[4] = {
                  static_cast<std::uint8_t>(binary[i]),
                  static_cast<std::uint8_t>(binary[i + 1]),
                  static_cast<std::uint8_t>(binary[i + 2]),
                  static_cast<std::uint8_t>(binary[i + 3])};
                i += 4;

                std::uint32_t i_u32 = i_u8[0] + (i_u8[1] << 8) + (i_u8[2] << 16) + (i_u8[3] << 24);

                code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u32), reinterpret_cast<std::byte*>(&i_u32) + 4);
                break;
            }
            default:
                throw interpreter_error(fmt::format("Unexpected opcode '{}' ({}) during decode.", to_string(static_cast<opcode>(instr)), static_cast<int>(instr)));
            }
        }
        std::size_t decoded_size = code.size() - decoded_offset;

        details.offset = decoded_offset;
        details.size = decoded_size;
    }

    return {std::move(header), std::move(code)};
}

value context::exec(const std::vector<std::byte>& binary, const function& f, const std::vector<value>& args)
{
    if(args.size() != 0)
    {
        throw std::runtime_error("context::exec: arguments not implemented.");
    }

    std::size_t offset = f.get_entry_point();
    if(offset >= binary.size())
    {
        throw interpreter_error(fmt::format("Entry point is outside the loaded code segment ({} >= {}).", offset, binary.size()));
    }

    exec_stack stack;
    const std::size_t function_end = f.get_entry_point() + f.get_size();
    while(offset < function_end)
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
        case opcode::fconst:
        {
            /* no out-of-bounds read possible, since this is checked during decode. */
            uint32_t i_u32 = *reinterpret_cast<const std::uint32_t*>(&binary[offset]);
            offset += 4;

            stack.push_i32(i_u32);
            break;
        } /* opcode::iconst, opcode::fconst */

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

    module_map.insert({name, decode(mod)});

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
            fmap.insert({it.name, function(desc.signature, std::get<function_details>(desc.details).offset, std::get<function_details>(desc.details).size)});
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

    return exec(mod_it->second.second, func_it->second, args);
}

}    // namespace slang::interpreter