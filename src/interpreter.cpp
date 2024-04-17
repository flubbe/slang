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
#include "package.h"
#include "utils.h"

namespace slang::interpreter
{

void context::decode_locals(function_descriptor& desc) const
{
    if(desc.native)
    {
        throw interpreter_error("Cannot decode locals for native function.");
    }
    auto& details = std::get<function_details>(desc.details);

    auto set_size = [](variable& v)
    {
        if(v.type == "i32")
        {
            v.size = sizeof(std::uint32_t);
        }
        else if(v.type == "f32")
        {
            v.size = sizeof(float);
        }
        else if(v.type == "str")
        {
            v.size = sizeof(std::string*);
        }
        else
        {
            throw interpreter_error(fmt::format("Size/offset resolution not implemented for type '{}'.", v.type));
        }
    };

    details.args_size = 0;
    details.locals_size = 0;

    std::size_t arg_count = desc.signature.arg_types.size();

    // arguments.
    for(std::size_t i = 0; i < arg_count; ++i)
    {
        auto& v = details.locals[i];

        v.offset = details.locals_size;
        set_size(v);
        details.locals_size += v.size;

        details.args_size += v.size;
    }

    // locals.
    for(std::size_t i = arg_count; i < details.locals.size(); ++i)
    {
        auto& v = details.locals[i];

        v.offset = details.locals_size;
        set_size(v);
        details.locals_size += v.size;
    }

    // return type
    if(desc.signature.return_type == "void")
    {
        details.return_size = 0;
    }
    if(desc.signature.return_type == "i32")
    {
        details.return_size = sizeof(std::uint32_t);
    }
    else if(desc.signature.return_type == "f32")
    {
        details.return_size = sizeof(float);
    }
    else if(desc.signature.return_type == "str")
    {
        details.return_size = sizeof(std::string*);
    }
    else
    {
        throw interpreter_error(fmt::format("Size/offset resolution not implemented for type '{}'.", desc.signature.return_type));
    }
}

void context::decode_instruction(const language_module& mod, archive& ar, std::byte instr, const function_details& details, std::vector<std::byte>& code) const
{
    switch(static_cast<opcode>(instr))
    {
    /* opcodes without arguments. */
    case opcode::iadd:
    case opcode::fadd:
    case opcode::isub:
    case opcode::fsub:
    case opcode::imul:
    case opcode::fmul:
    case opcode::idiv:
    case opcode::fdiv:
    case opcode::ret:
    case opcode::iret:
    case opcode::fret:
    case opcode::sret:
        break;
    /* opcodes with one 4-byte argument. */
    case opcode::iconst:
    case opcode::fconst:
    {
        std::uint32_t i_u32;
        ar & i_u32;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u32), reinterpret_cast<std::byte*>(&i_u32) + sizeof(i_u32));
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
    /** invoke. */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        // resolve to address.
        if(i.i < 0)
        {
            // TODO
            throw interpreter_error("interpreter::decode_instruction: invoke not implemented for imports.");
        }

        auto& symbol = mod.get_header().exports[i.i];
        if(symbol.type != symbol_type::function)
        {
            throw interpreter_error(fmt::format("Cannot resolve call: Header entry at index {} is not a function.", i.i));
        }
        auto& desc = std::get<function_descriptor>(symbol.desc);

        if(desc.native)
        {
            // TODO
            throw interpreter_error("interpreter::decode_instruction: native function calls not yet implemented.");
        }

        const function_descriptor* desc_ptr = &desc;
        code.insert(code.end(), reinterpret_cast<const std::byte*>(&desc_ptr), reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));
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

language_module context::decode(const language_module& mod) const
{
    if(mod.is_decoded())
    {
        throw interpreter_error("Tried to decode a module that already is decoded.");
    }

    module_header header = mod.get_header();
    memory_read_archive ar{mod.get_binary(), true, slang::endian::little};

    /*
     * arguments and locals.
     */
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

        decode_locals(desc);
    }

    // store header in decoded module.
    language_module decoded_module{std::move(header)};

    /*
     * instructions.
     */
    std::vector<std::byte> code;
    for(auto& it: decoded_module.header.exports)
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

        std::size_t bytecode_end = details.offset + details.size;

        ar.seek(details.offset);
        details.offset = code.size();

        std::byte instr;

        while(ar.tell() < bytecode_end)
        {
            ar & instr;
            code.push_back(instr);

            decode_instruction(decoded_module, ar, instr, details, code);
        }

        details.size = code.size() - details.offset;
    }

    decoded_module.set_binary(std::move(code));
    decoded_module.decoded = true;

    return decoded_module;
}

opcode context::exec(const language_module& mod,
                     std::size_t entry_point,
                     std::size_t size,
                     stack_frame& frame)
{
    if(!mod.is_decoded())
    {
        throw interpreter_error("Cannot execute function using non-decoded module.");
    }

    const std::vector<std::byte>& binary = mod.get_binary();

    std::size_t offset = entry_point;
    if(offset >= binary.size())
    {
        throw interpreter_error(fmt::format("Entry point is outside the loaded code segment ({} >= {}).", offset, binary.size()));
    }

    const std::size_t function_end = offset + size;
    while(offset < function_end)
    {
        auto instr = binary[offset];
        ++offset;

        // return.
        if(instr >= static_cast<std::byte>(opcode::ret) && instr <= static_cast<std::byte>(opcode::sret))
        {
            return static_cast<opcode>(instr);
        }

        if(offset == function_end)
        {
            throw interpreter_error(fmt::format("Execution reached function boundary."));
        }

        switch(static_cast<opcode>(instr))
        {
        case opcode::iadd:
        {
            frame.stack.push_i32(frame.stack.pop_i32() + frame.stack.pop_i32());
            break;
        } /* opcode::iadd */
        case opcode::isub:
        {
            frame.stack.push_i32(-frame.stack.pop_i32() + frame.stack.pop_i32());
            break;
        } /* opcode::isub */
        case opcode::imul:
        {
            frame.stack.push_i32(frame.stack.pop_i32() * frame.stack.pop_i32());
            break;
        } /* opcode::imul */
        case opcode::idiv:
        {
            std::int32_t divisor = frame.stack.pop_i32();
            if(divisor == 0)
            {
                throw interpreter_error("Division by zero.");
            }
            std::int32_t dividend = frame.stack.pop_i32();
            frame.stack.push_i32(dividend / divisor);
            break;
        } /* opcode::idiv */
        case opcode::fadd:
        {
            frame.stack.push_f32(frame.stack.pop_f32() + frame.stack.pop_f32());
            break;
        } /* opcode::fadd */
        case opcode::fsub:
        {
            frame.stack.push_f32(-frame.stack.pop_f32() + frame.stack.pop_f32());
            break;
        } /* opcode::fsub */
        case opcode::fmul:
        {
            frame.stack.push_f32(frame.stack.pop_f32() * frame.stack.pop_f32());
            break;
        } /* opcode::fmul */
        case opcode::fdiv:
        {
            float divisor = frame.stack.pop_f32();
            if(divisor == 0)
            {
                throw interpreter_error("Division by zero.");
            }
            float dividend = frame.stack.pop_f32();
            frame.stack.push_f32(dividend / divisor);
            break;
        } /* opcode::fdiv */
        case opcode::iconst:
        case opcode::fconst:
        {
            /* no out-of-bounds read possible, since this is checked during decode. */
            std::uint32_t i_u32 = *reinterpret_cast<const std::uint32_t*>(&binary[offset]);
            offset += sizeof(std::uint32_t);

            frame.stack.push_i32(i_u32);
            break;
        } /* opcode::iconst, opcode::fconst */
        case opcode::sconst:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::uint64_t);

            if(i < 0 || i >= frame.string_table.size())
            {
                throw interpreter_error(fmt::format("Invalid index '{}' into string table.", i));
            }

            frame.stack.push_addr(&frame.string_table[i]);
            break;
        } /* opcode::sconst */
        case opcode::iload:
        case opcode::fload:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::uint64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            frame.stack.push_i32(*reinterpret_cast<std::uint32_t*>(&frame.locals[i]));
            break;
        } /* opcode::iload, opcode::fload */
        case opcode::sload:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::uint64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            frame.stack.push_addr(*reinterpret_cast<std::string**>(&frame.locals[i]));
            break;
        } /* opcode::sload */
        case opcode::invoke:
        {
            function_descriptor* const* desc_ptr = reinterpret_cast<function_descriptor* const*>(&binary[offset]);
            offset += sizeof(desc_ptr);

            const function_descriptor* desc = *desc_ptr;
            if(desc->native)
            {
                throw interpreter_error("opcode::invoke not implemented for native functions.");
            }

            auto& details = std::get<function_details>(desc->details);

            stack_frame callee_frame{frame.string_table, details.locals_size};
            auto* args_start = reinterpret_cast<std::byte*>(frame.stack.end(details.args_size));
            std::copy(args_start, args_start + details.args_size, callee_frame.locals.data());
            frame.stack.discard(details.args_size);

            exec(mod, details.offset, details.size, callee_frame);

            if(callee_frame.stack.size() != details.return_size)
            {
                throw interpreter_error(fmt::format("Expected {} bytes to be returned from function call, got {}.", details.return_size, callee_frame.stack.size()));
            }
            frame.stack.push_stack(callee_frame.stack);
            break;
        } /* opcode::invoke */

        default:
            throw interpreter_error(fmt::format("Opcode '{}' ({}) not implemented.", to_string(static_cast<opcode>(instr)), instr));
        }
    }

    throw interpreter_error("Out of bounds code read.");
}

value context::exec(const language_module& mod,
                    const function& f,
                    const std::vector<value>& args)
{
    /*
     * allocate locals and decode arguments.
     */
    stack_frame frame{mod.get_header().strings, f.locals_size};
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

            *reinterpret_cast<std::int32_t*>(&frame.locals[offset]) = std::get<int>(args[i]);

            offset += sizeof(std::int32_t);
        }
        else if(arg_type == "f32")
        {
            if(offset + sizeof(float) > f.locals_size)
            {
                throw interpreter_error("Stack overflow during argument allocation.");
            }

            *reinterpret_cast<float*>(&frame.locals[offset]) = std::get<float>(args[i]);

            offset += sizeof(float);
        }
        else if(arg_type == "str")
        {
            if(offset + sizeof(std::string*) > f.locals_size)
            {
                throw interpreter_error("Stack overflow during argument allocation.");
            }

            local_strings.emplace_back(std::get<std::string>(args[i]));
            *reinterpret_cast<std::string**>(&frame.locals[offset]) = &local_strings.back();

            offset += sizeof(std::string*);
        }
        else
        {
            throw interpreter_error(fmt::format("Argument type '{}' not implemented.", arg_type));
        }
    }

    /*
     * Execute the function.
     */
    opcode ret_opcode = exec(mod, f.get_entry_point(), f.get_size(), frame);

    /*
     * Decode return value.
     */
    value ret;
    if(ret_opcode == opcode::iret)
    {
        ret = frame.stack.pop_i32();
    }
    else if(ret_opcode == opcode::fret)
    {
        ret = frame.stack.pop_f32();
    }
    else if(ret_opcode == opcode::sret)
    {
        ret = std::string{*frame.stack.pop_addr<std::string>()};
    }
    else
    {
        throw interpreter_error(fmt::format("Invalid return opcode '{}' ({}).", to_string(ret_opcode), static_cast<int>(ret_opcode)));
    }

    return ret;
}

void context::load_module(const std::string& name, const language_module& mod)
{
    if(module_map.find(name) != module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' already loaded.", name));
    }

    module_map.insert({name, decode(mod)});
    const module_header& decoded_header = module_map[name].get_header();

    // resolve dependencies.
    for(auto& it: decoded_header.imports)
    {
        if(it.type == symbol_type::package)
        {
            // packages are loaded while resolving other symbols.
            continue;
        }

        // resolve the symbol's package.
        if(it.package_index >= decoded_header.imports.size())
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' has invalid package index.", it.name));
        }
        else if(decoded_header.imports[it.package_index].type != symbol_type::package)
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' refers to non-package import entry.", it.name));
        }

        std::string import_name = decoded_header.imports[it.package_index].name;
        slang::utils::replace_all(import_name, package::delimiter, "/");

        fs::path fs_path = fs::path{import_name}.replace_extension(package::module_ext);
        fs::path resolved_path = file_mgr.resolve(fs_path);
        std::unique_ptr<slang::file_archive> ar = file_mgr.open(resolved_path, slang::file_manager::open_mode::read);

        language_module import_mod;
        (*ar) & import_mod;

        load_module(import_name, import_mod);

        // find the imported symbol.
        const module_header& import_header = module_map[import_name].get_header();
        auto exp_it = std::find_if(import_header.exports.begin(), import_header.exports.end(),
                                   [&it](const exported_symbol& exp) -> bool
                                   {
                                       return exp.name == it.name;
                                   });
        if(exp_it == import_header.exports.end())
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Symbol '{}' is not exported by module '{}'.", it.name, import_name));
        }
        if(exp_it->type != it.type)
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Symbol '{}' from module '{}' has wrong type (expected '{}', got '{}').", it.name, import_name, slang::to_string(it.type), slang::to_string(exp_it->type)));
        }

        // TODO

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
            auto& details = std::get<native_function_details>(desc.details);

            // TODO

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

    return exec(mod_it->second, func_it->second, args);
}

}    // namespace slang::interpreter