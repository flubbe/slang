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
    memory_read_archive ar{mod.get_binary(), true, slang::endian::little};

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

        /*
         * arguments and locals.
         */

        details.locals_size = 0;
        for(auto& it: details.locals)
        {
            it.offset = details.locals_size;

            if(it.type == "i32")
            {
                it.size = sizeof(std::uint32_t);
            }
            else if(it.type == "f32")
            {
                it.size = sizeof(float);
            }
            else if(it.type == "str")
            {
                it.size = sizeof(std::string*);
            }
            else
            {
                throw interpreter_error(fmt::format("Size/offset resolution not implemented for type '{}'.", it.type));
            }

            details.locals_size += it.size;
        }

        /*
         * instructions.
         */

        std::size_t decoded_offset = code.size();
        std::size_t end_offset = details.offset + details.size;

        ar.seek(details.offset);
        while(ar.tell() < end_offset)
        {
            std::byte instr;
            ar & instr;

            code.push_back(instr);

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
                std::uint32_t i_u32;
                ar & i_u32;

                code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u32), reinterpret_cast<std::byte*>(&i_u32) + 4);
                break;
            }
            /* opcodes with one VLE integer. */
            case opcode::sconst:
            {
                vle_int i;
                ar & i;

                code.insert(code.end(), reinterpret_cast<std::byte*>(&i.i), reinterpret_cast<std::byte*>(&i.i) + sizeof(i.i));
                break;
            }
            /* opcodes that need to resolve a variable. */
            case opcode::iload:
            case opcode::fload:
            case opcode::sload:
            case opcode::istore:
            case opcode::fstore:
            case opcode::sstore:
            {
                vle_int i;
                ar & i;

                if(i.i < 0 || i.i >= details.locals.size())
                {
                    throw interpreter_error(fmt::format("index '{}' for argument or local outside of valid range 0-{}.", i.i, details.locals.size()));
                }

                std::int64_t offset = details.locals[i.i].offset;
                code.insert(code.end(), reinterpret_cast<std::byte*>(&offset), reinterpret_cast<std::byte*>(&offset) + sizeof(offset));

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

value context::exec(const std::vector<std::string>& string_table, const std::vector<std::byte>& binary, const function& f, const std::vector<value>& args)
{
    // allocate locals and decode arguments.
    std::vector<std::byte> locals{f.locals_size};
    std::vector<std::string> local_strings;

    auto& arg_types = f.get_signature().arg_types;
    if(arg_types.size() != args.size())
    {
        throw interpreter_error(fmt::format("Arguments for function '{}' do not match: got {}, expected {}.", arg_types.size(), args.size()));
    }

    std::size_t offset = 0;
    for(std::size_t i = 0; i < args.size(); ++i)
    {
        auto& arg_type = arg_types[i];
        if(arg_type == "i32")
        {
            if(offset + sizeof(std::int32_t) > f.locals_size)
            {
                throw interpreter_error("Stack overflow during argument allocation.");
            }

            *reinterpret_cast<std::int32_t*>(&locals[offset]) = std::get<int>(args[i]);

            offset += sizeof(std::int32_t);
        }
        else if(arg_type == "f32")
        {
            if(offset + sizeof(float) > f.locals_size)
            {
                throw interpreter_error("Stack overflow during argument allocation.");
            }

            *reinterpret_cast<float*>(&locals[offset]) = std::get<float>(args[i]);

            offset += sizeof(float);
        }
        else if(arg_type == "str")
        {
            if(offset + sizeof(std::string*) > f.locals_size)
            {
                throw interpreter_error("Stack overflow during argument allocation.");
            }

            local_strings.emplace_back(std::get<std::string>(args[i]));
            *reinterpret_cast<std::string**>(&locals[offset]) = &local_strings.back();

            offset += sizeof(std::string*);
        }
        else
        {
            throw interpreter_error(fmt::format("Argument type '{}' not implemented.", arg_type));
        }
    }

    // execute the instructions.
    offset = f.get_entry_point();
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

        if(offset == function_end)
        {
            throw interpreter_error(fmt::format("Execution reached function boundary."));
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
            std::uint32_t i_u32 = *reinterpret_cast<const std::uint32_t*>(&binary[offset]);
            offset += sizeof(std::uint32_t);

            stack.push_i32(i_u32);
            break;
        } /* opcode::iconst, opcode::fconst */
        case opcode::sconst:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::uint64_t);

            if(i < 0 || i >= string_table.size())
            {
                throw interpreter_error(fmt::format("Invalid index '{}' into string table.", i));
            }

            stack.push_addr(&string_table[i]);
            break;
        } /* opcode::sconst */
        case opcode::iload:
        case opcode::fload:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::uint64_t);

            if(i < 0)
            {
                // TODO globals?
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::uint32_t) > locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            stack.push_i32(*reinterpret_cast<std::uint32_t*>(&locals[i]));
            break;
        } /* opcode::iload, opcode::fload */

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
    else if(signature.return_type == "str")
    {
        return {std::string{*stack.pop_addr<std::string>()}};
    }

    if(signature.return_type != "void")
    {
        throw interpreter_error(fmt::format("Expected result type 'void', got '{}'", signature.return_type));
    }

    // return 'void'.
    return {};
}

void context::load_module(const std::string& name, const language_module& mod)
{
    if(module_map.find(name) != module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' already loaded.", name));
    }

    auto [decoded_header, decoded_binary] = decode(mod);
    module_map.insert({name, {decoded_header, decoded_binary}});

    // resolve dependencies.
    for(auto& it: decoded_header.imports)
    {
        throw interpreter_error("context::load_module: import resolution not implemented.");
    }

    // populate function map.
    std::unordered_map<std::string, function> fmap;
    for(auto& it: decoded_header.exports)
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
            auto& details = std::get<function_details>(desc.details);
            fmap.insert({it.name, function(desc.signature, details.offset, details.size, details.locals_size)});
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

    return exec(mod_it->second.first.strings, mod_it->second.second, func_it->second, args);
}

}    // namespace slang::interpreter