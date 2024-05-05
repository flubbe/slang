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

/**
 * Return the size of a built-in type.
 *
 * @param type The type name.
 * @return Returns the type size.
 * @throws Throws an `interpreter_error` if the type is not known.
 */
static std::size_t get_type_size(const std::pair<std::string, std::optional<std::size_t>>& type)
{
    auto& name = std::get<0>(type);
    std::size_t length = std::get<1>(type).has_value() ? *std::get<1>(type) : 1;

    if(name == "void")
    {
        return 0;
    }
    else if(name == "i32")
    {
        return sizeof(std::uint32_t) * length;
    }
    else if(name == "f32")
    {
        return sizeof(float) * length;
    }
    else if(name == "str")
    {
        return sizeof(std::string*) * length;
    }

    throw interpreter_error(fmt::format("Size resolution not implemented for type '{}'.", name));
}

/**
 * Generate the return opcode from the signature's return type for native functions.
 *
 * @param return_type The return type.
 * @returns A return opcode, together with an array size. An array size of 0 indicates "no array".
 * @throws Throws an `interpreter_error` if the `return_type` is invalid.
 */
static std::pair<opcode, std::int64_t> get_return_opcode(const std::pair<std::string, std::optional<std::size_t>>& return_type)
{
    auto& name = std::get<0>(return_type);
    std::size_t length = std::get<1>(return_type).has_value() ? *std::get<1>(return_type) : 0;

    if(name == "void")
    {
        return std::make_pair(opcode::ret, 0);
    }
    else if(name == "i32")
    {
        return std::make_pair(opcode::iret, length);
    }
    else if(name == "f32")
    {
        return std::make_pair(opcode::fret, length);
    }
    else if(name == "str")
    {
        return std::make_pair(opcode::sret, length);
    }

    throw interpreter_error(fmt::format("Type '{}' has no return opcode.", name));
}

/**
 * Calculate the stack size delta from a function's signature.
 *
 * @param s The signature.
 * @returns The stack size delta.
 */
static int32_t get_stack_delta(const function_signature& s)
{
    std::int32_t return_type_size = get_type_size(s.return_type);
    std::int32_t arg_size = 0;
    for(auto& it: s.arg_types)
    {
        arg_size += get_type_size(it);
    }
    return return_type_size - arg_size;
}

/*
 * function implementation.
 */

function::function(function_signature signature, std::size_t entry_point, std::size_t size, std::size_t locals_size, std::size_t stack_size)
: signature{std::move(signature)}
, native{false}
, entry_point_or_function{entry_point}
, size{size}
, locals_size{locals_size}
, stack_size{stack_size}
{
    auto [oc, length] = ::slang::interpreter::get_return_opcode(this->signature.return_type);
    ret_opcode = oc;
    ret_array_length = length;
}

function::function(function_signature signature, std::function<void(operand_stack&)> func)
: signature{std::move(signature)}
, native{true}
, entry_point_or_function{std::move(func)}
{
    auto [oc, length] = ::slang::interpreter::get_return_opcode(this->signature.return_type);
    ret_opcode = oc;
    ret_array_length = length;
}

/*
 * context implementation.
 */

void context::decode_locals(function_descriptor& desc) const
{
    if(desc.native)
    {
        throw interpreter_error("Cannot decode locals for native function.");
    }
    auto& details = std::get<function_details>(desc.details);

    details.args_size = 0;
    details.locals_size = 0;

    std::size_t arg_count = desc.signature.arg_types.size();

    // arguments.
    for(std::size_t i = 0; i < arg_count; ++i)
    {
        auto& v = details.locals[i];

        v.offset = details.locals_size;
        v.size = get_type_size({v.type, 1});

        auto total_size = v.size * v.array_length.i;
        details.locals_size += total_size;
        details.args_size += total_size;
    }

    // locals.
    for(std::size_t i = arg_count; i < details.locals.size(); ++i)
    {
        auto& v = details.locals[i];

        v.offset = details.locals_size;
        v.size = get_type_size({v.type, 1});
        details.locals_size += v.size * v.array_length.i;
    }

    // return type
    details.return_size = get_type_size(desc.signature.return_type);
}

std::int32_t context::decode_instruction(language_module& mod, archive& ar, std::byte instr, const function_details& details, std::vector<std::byte>& code) const
{
    switch(static_cast<opcode>(instr))
    {
    /* opcodes without arguments. */
    case opcode::iadd: [[fallthrough]];
    case opcode::fadd: [[fallthrough]];
    case opcode::isub: [[fallthrough]];
    case opcode::fsub: [[fallthrough]];
    case opcode::imul: [[fallthrough]];
    case opcode::fmul: [[fallthrough]];
    case opcode::idiv: [[fallthrough]];
    case opcode::fdiv: [[fallthrough]];
    case opcode::imod: [[fallthrough]];
    case opcode::iand: [[fallthrough]];
    case opcode::ior: [[fallthrough]];
    case opcode::ixor: [[fallthrough]];
    case opcode::ishl: [[fallthrough]];
    case opcode::ishr: [[fallthrough]];
    case opcode::icmpl: [[fallthrough]];
    case opcode::fcmpl: [[fallthrough]];
    case opcode::icmple: [[fallthrough]];
    case opcode::fcmple: [[fallthrough]];
    case opcode::icmpg: [[fallthrough]];
    case opcode::fcmpg: [[fallthrough]];
    case opcode::icmpge: [[fallthrough]];
    case opcode::fcmpge: [[fallthrough]];
    case opcode::icmpeq: [[fallthrough]];
    case opcode::fcmpeq: [[fallthrough]];
    case opcode::icmpne: [[fallthrough]];
    case opcode::fcmpne:
        return -static_cast<std::int32_t>(sizeof(std::uint32_t));    // same size for all (since sizeof(float) == sizeof(std::uint32_t))
    case opcode::i2f: [[fallthrough]];
    case opcode::f2i: [[fallthrough]];
    case opcode::ret:
        return 0;
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32;
        ar & i_u32;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u32), reinterpret_cast<std::byte*>(&i_u32) + sizeof(i_u32));
        return sizeof(std::uint32_t);
    }
    /* opcodes with one VLE integer. */
    case opcode::iret: [[fallthrough]];
    case opcode::fret: [[fallthrough]];
    case opcode::sret: [[fallthrough]];
    case opcode::sconst:
    {
        vle_int i;
        ar & i;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i.i), reinterpret_cast<std::byte*>(&i.i) + sizeof(i.i));
        return sizeof(std::string*);
    }
    case opcode::label:
    {
        vle_int i;
        ar & i;

        // store jump target for later resolution.
        mod.jump_targets.insert({i.i, code.size()});
        return 0;
    }
    case opcode::jmp:
    {
        vle_int i;
        ar & i;

        // store jump origin for later resolution and write zeros instead.
        std::size_t z = 0;
        mod.jump_origins.insert({code.size(), i.i});
        code.insert(code.end(), reinterpret_cast<std::byte*>(&z), reinterpret_cast<std::byte*>(&z) + sizeof(z));
        return 0;
    }
    /* opcodes with two VLE integers. */
    case opcode::jnz:
    {
        vle_int i1, i2;
        ar & i1 & i2;

        // store jump origin for later resolution and write zeros instead.
        std::size_t z = 0;
        mod.jump_origins.insert({code.size(), i1.i});
        code.insert(code.end(), reinterpret_cast<std::byte*>(&z), reinterpret_cast<std::byte*>(&z) + sizeof(z));

        mod.jump_origins.insert({code.size(), i2.i});
        code.insert(code.end(), reinterpret_cast<std::byte*>(&z), reinterpret_cast<std::byte*>(&z) + sizeof(z));
        return 0;
    }
    /** invoke. */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        // resolve to address.
        if(i.i < 0)
        {
            auto& imp_symbol = mod.header.imports[-i.i - 1];
            if(imp_symbol.type != symbol_type::function)
            {
                throw interpreter_error(fmt::format("Cannot resolve call: Import header at index {} does not refer to a function.", -i.i - 1));
            }

            auto& exp_symbol = std::get<exported_symbol*>(imp_symbol.export_reference);
            if(exp_symbol->type != imp_symbol.type)
            {
                throw interpreter_error(fmt::format("Cannot resolve call: Export type for '{}' does not match import type ({} != {}).", imp_symbol.name, slang::to_string(exp_symbol->type), slang::to_string(imp_symbol.type)));
            }

            auto& desc = std::get<function_descriptor>(exp_symbol->desc);

            const language_module* mod_ptr = std::get<const language_module*>(mod.header.imports[imp_symbol.package_index].export_reference);
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&mod_ptr), reinterpret_cast<const std::byte*>(&mod_ptr) + sizeof(mod_ptr));

            const function_descriptor* desc_ptr = &desc;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&desc_ptr), reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));

            return get_stack_delta(desc.signature);
        }
        else
        {
            auto& exp_symbol = mod.header.exports[i.i];
            if(exp_symbol.type != symbol_type::function)
            {
                throw interpreter_error(fmt::format("Cannot resolve call: Header entry at index {} is not a function.", i.i));
            }
            auto& desc = std::get<function_descriptor>(exp_symbol.desc);

            const language_module* mod_ptr = &mod;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&mod_ptr), reinterpret_cast<const std::byte*>(&mod_ptr) + sizeof(mod_ptr));

            const function_descriptor* desc_ptr = &desc;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&desc_ptr), reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));

            return get_stack_delta(desc.signature);
        }
    }
    /* opcodes that need to resolve a variable. */
    case opcode::iloada: [[fallthrough]];
    case opcode::floada: [[fallthrough]];
    case opcode::sloada: [[fallthrough]];
    case opcode::istorea: [[fallthrough]];
    case opcode::fstorea: [[fallthrough]];
    case opcode::sstorea: [[fallthrough]];
    case opcode::iload: [[fallthrough]];
    case opcode::fload: [[fallthrough]];
    case opcode::sload: [[fallthrough]];
    case opcode::istore: [[fallthrough]];
    case opcode::fstore: [[fallthrough]];
    case opcode::sstore:
    {
        vle_int i;
        ar & i;

        if(i.i < 0 || i.i >= details.locals.size())
        {
            throw interpreter_error(fmt::format("Index '{}' for argument or local outside of valid range 0-{}.", i.i, details.locals.size()));
        }

        std::int64_t offset = details.locals[i.i].offset;
        code.insert(code.end(), reinterpret_cast<std::byte*>(&offset), reinterpret_cast<std::byte*>(&offset) + sizeof(offset));

        // return correct size.
        bool is_store = (static_cast<opcode>(instr) == opcode::istorea)
                        || (static_cast<opcode>(instr) == opcode::fstorea)
                        || (static_cast<opcode>(instr) == opcode::sstorea)
                        || (static_cast<opcode>(instr) == opcode::istore)
                        || (static_cast<opcode>(instr) == opcode::fstore)
                        || (static_cast<opcode>(instr) == opcode::sstore);
        bool is_string = (static_cast<opcode>(instr) == opcode::sloada)
                         || (static_cast<opcode>(instr) == opcode::sstorea)
                         || (static_cast<opcode>(instr) == opcode::sload)
                         || (static_cast<opcode>(instr) == opcode::sstore);
        if(!is_string)
        {
            return is_store ? -static_cast<std::int32_t>(sizeof(std::uint32_t)) : sizeof(std::uint32_t);    // same size for i32/f32 (since sizeof(float) == sizeof(std::uint32_t))
        }
        else
        {
            return is_store ? -static_cast<std::int32_t>(sizeof(std::string*)) : sizeof(std::string*);
        }
    }
    default:
        throw interpreter_error(fmt::format("Unexpected opcode '{}' ({}) during decode.", to_string(static_cast<opcode>(instr)), static_cast<int>(instr)));
    }
}

std::unique_ptr<language_module> context::decode(const language_module& mod)
{
    if(mod.is_decoded())
    {
        throw interpreter_error("Tried to decode a module that already is decoded.");
    }

    module_header header = mod.get_header();
    memory_read_archive ar{mod.get_binary(), true, slang::endian::little};

    /*
     * resolve imports.
     */
    for(auto& it: header.imports)
    {
        if(it.type == symbol_type::package)
        {
            // packages are loaded while resolving other symbols.
            continue;
        }

        // resolve the symbol's package.
        if(it.package_index >= header.imports.size())
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' has invalid package index.", it.name));
        }
        else if(header.imports[it.package_index].type != symbol_type::package)
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' refers to non-package import entry.", it.name));
        }

        std::string import_name = header.imports[it.package_index].name;
        slang::utils::replace_all(import_name, package::delimiter, "/");

        fs::path fs_path = fs::path{import_name}.replace_extension(package::module_ext);
        fs::path resolved_path = file_mgr.resolve(fs_path);
        std::unique_ptr<slang::file_archive> ar = file_mgr.open(resolved_path, slang::file_manager::open_mode::read);

        language_module import_mod;
        (*ar) & import_mod;

        load_module(import_name, import_mod);

        // find the imported symbol.
        module_header& import_header = module_map[import_name]->header;
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

        it.export_reference = &(*exp_it);

        // resolve symbol
        if(it.type != symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<function_descriptor>(exp_it->desc);
        if(desc.native)
        {
            // resolve native function.
            auto& details = std::get<native_function_details>(desc.details);
            auto mod_it = native_function_map.find(details.library_name);
            if(mod_it == native_function_map.end())
            {
                throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (library not found).", exp_it->name, details.library_name));
            }

            auto func_it = mod_it->second.find(exp_it->name);
            if(func_it == mod_it->second.end())
            {
                throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (function not found).", exp_it->name, details.library_name));
            }

            details.func = func_it->second;
        }
    }

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
    auto decoded_module = std::make_unique<language_module>(std::move(header));

    /*
     * instructions.
     */
    std::vector<std::byte> code;
    for(auto& it: decoded_module->header.exports)
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

        std::int32_t stack_size = 0;
        std::size_t max_stack_size = 0;

        while(ar.tell() < bytecode_end)
        {
            ar & instr;

            // don't store non-executable instructions.
            if(static_cast<opcode>(instr) != opcode::label)
            {
                code.push_back(instr);
            }

            stack_size += decode_instruction(*decoded_module, ar, instr, details, code);
            if(stack_size < 0)
            {
                throw interpreter_error("Error during decode: Got negative stack size.");
            }

            if(stack_size > max_stack_size)
            {
                max_stack_size = stack_size;
            }
        }

        details.size = code.size() - details.offset;
        details.stack_size = max_stack_size;

        // jumps target resolution.
        for(auto& [origin, id]: decoded_module->jump_origins)
        {
            auto target_it = decoded_module->jump_targets.find(id);
            if(target_it == decoded_module->jump_targets.end())
            {
                throw interpreter_error(fmt::format("Unable to resolve jump target for label '{}'.", id));
            }
            *reinterpret_cast<std::size_t*>(&code[origin]) = target_it->second;
        }
    }

    decoded_module->set_binary(std::move(code));
    decoded_module->decoded = true;

    return decoded_module;
}

std::pair<opcode, std::int64_t> context::exec(const language_module& mod,
                                              std::size_t entry_point,
                                              std::size_t size,
                                              stack_frame& frame)
{
    if(!mod.is_decoded())
    {
        throw interpreter_error("Cannot execute function using non-decoded module.");
    }

    ++call_stack_level;
    if(call_stack_level > max_call_stack_depth)
    {
        throw interpreter_error(fmt::format("Function call exceeded maximum call stack depth ({}).", max_call_stack_depth));
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
            --call_stack_level;

            std::int64_t array_length = 0;
            if(instr != static_cast<std::byte>(opcode::ret))
            {
                // read array length.
                array_length = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            }

            return std::make_pair(static_cast<opcode>(instr), array_length);
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
        case opcode::imod:
        {
            std::int32_t divisor = frame.stack.pop_i32();
            if(divisor == 0)
            {
                throw interpreter_error("Division by zero.");
            }
            std::int32_t dividend = frame.stack.pop_i32();
            frame.stack.push_i32(dividend % divisor);
            break;
        } /* opcode::imod */
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
        case opcode::i2f:
        {
            frame.stack.push_f32(frame.stack.pop_i32());
            break;
        } /* opcode::i2f */
        case opcode::f2i:
        {
            frame.stack.push_i32(frame.stack.pop_f32());
            break;
        } /* opcode::f2i */
        case opcode::iconst: [[fallthrough]];
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
        case opcode::iloada: [[fallthrough]];
        case opcode::floada:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            std::size_t array_offset = frame.stack.pop_i32() * sizeof(std::int32_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + array_offset + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Invalid memory access.");
            }

            frame.stack.push_i32(*reinterpret_cast<std::uint32_t*>(&frame.locals[i + array_offset]));
            break;
        } /* opcode::iloada, opcode::floada */
        case opcode::sloada:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            std::size_t array_offset = frame.stack.pop_i32() * sizeof(std::string*);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + array_offset + sizeof(std::string*) > frame.locals.size())
            {
                throw interpreter_error("Invalid memory access.");
            }

            frame.stack.push_addr(*reinterpret_cast<std::string**>(&frame.locals[i + array_offset]));
            break;
        } /* opcode::sloada */
        case opcode::istorea: [[fallthrough]];
        case opcode::fstorea:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            std::size_t array_offset = frame.stack.pop_i32() * sizeof(std::int32_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + array_offset + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            *reinterpret_cast<std::uint32_t*>(&frame.locals[i + array_offset]) = frame.stack.pop_i32();
            break;
        } /* opcode::istorea, opcode::fstorea */
        case opcode::sstorea:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            std::size_t array_offset = frame.stack.pop_i32() * sizeof(std::int32_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + array_offset + sizeof(std::string*) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            *reinterpret_cast<std::string**>(&frame.locals[i + array_offset]) = frame.stack.pop_addr<std::string>();
            break;
        } /* opcode::sstorea */
        case opcode::iload: [[fallthrough]];
        case opcode::fload:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Invalid memory access.");
            }

            frame.stack.push_i32(*reinterpret_cast<std::uint32_t*>(&frame.locals[i]));
            break;
        } /* opcode::iload, opcode::fload */
        case opcode::sload:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::string*) > frame.locals.size())
            {
                throw interpreter_error("Invalid memory access.");
            }

            frame.stack.push_addr(*reinterpret_cast<std::string**>(&frame.locals[i]));
            break;
        } /* opcode::sload */
        case opcode::istore: [[fallthrough]];
        case opcode::fstore:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::uint32_t) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            *reinterpret_cast<std::uint32_t*>(&frame.locals[i]) = frame.stack.pop_i32();
            break;
        } /* opcode::istore, opcode::fstore */
        case opcode::sstore:
        {
            std::int64_t i = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::int64_t);

            if(i < 0)
            {
                throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
            }

            if(i + sizeof(std::string*) > frame.locals.size())
            {
                throw interpreter_error("Stack overflow.");
            }

            *reinterpret_cast<std::string**>(&frame.locals[i]) = frame.stack.pop_addr<std::string>();
            break;
        } /* opcode::sstore */
        case opcode::invoke:
        {
            language_module* const* callee_mod = reinterpret_cast<language_module* const*>(&binary[offset]);
            offset += sizeof(callee_mod);

            function_descriptor* const* desc_ptr = reinterpret_cast<function_descriptor* const*>(&binary[offset]);
            offset += sizeof(desc_ptr);

            const function_descriptor* desc = *desc_ptr;
            if(desc->native)
            {
                auto& details = std::get<native_function_details>(desc->details);
                if(!details.func)
                {
                    throw interpreter_error("Tried to invoke unresolved native function.");
                }

                details.func(frame.stack);
            }
            else
            {
                auto& details = std::get<function_details>(desc->details);

                stack_frame callee_frame{frame.string_table, details.locals_size, details.stack_size};
                auto* args_start = reinterpret_cast<std::byte*>(frame.stack.end(details.args_size));
                std::copy(args_start, args_start + details.args_size, callee_frame.locals.data());
                frame.stack.discard(details.args_size);

                exec(mod, details.offset, details.size, callee_frame);

                if(callee_frame.stack.size() != details.return_size)
                {
                    throw interpreter_error(fmt::format("Expected {} bytes to be returned from function call, got {}.", details.return_size, callee_frame.stack.size()));
                }
                frame.stack.push_stack(callee_frame.stack);
            }
            break;
        } /* opcode::invoke */
        case opcode::iand:
        {
            frame.stack.push_i32(frame.stack.pop_i32() & frame.stack.pop_i32());
            break;
        } /* opcode::iand */
        case opcode::ior:
        {
            frame.stack.push_i32(frame.stack.pop_i32() | frame.stack.pop_i32());
            break;
        } /* opcode::ior */
        case opcode::ixor:
        {
            frame.stack.push_i32(frame.stack.pop_i32() ^ frame.stack.pop_i32());
            break;
        } /* opcode::ixor */
        case opcode::ishl:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.
            std::int32_t s = frame.stack.pop_i32();
            std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);

            frame.stack.push_i32(s_u32 << a_u32);
            break;
        } /* opcode::ishl */
        case opcode::ishr:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.
            std::int32_t s = frame.stack.pop_i32();
            std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);

            frame.stack.push_i32(s_u32 >> a_u32);
            break;
        } /* opcode::ishr */
        case opcode::icmpl:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b < a);
            break;
        } /* opcode::icmpl */
        case opcode::fcmpl:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b < a);
            break;
        } /* opcode::fcmpl */
        case opcode::icmple:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b <= a);
            break;
        } /* opcode::icmple */
        case opcode::fcmple:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b <= a);
            break;
        } /* opcode::fcmple */
        case opcode::icmpg:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b > a);
            break;
        } /* opcode::icmpg */
        case opcode::fcmpg:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b > a);
            break;
        } /* opcode::fcmpg */
        case opcode::icmpge:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b >= a);
            break;
        } /* opcode::icmpge */
        case opcode::fcmpge:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b >= a);
            break;
        } /* opcode::fcmpge */
        case opcode::icmpeq:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b == a);
            break;
        } /* opcode::icmpeq */
        case opcode::fcmpeq:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b == a);
            break;
        } /* opcode::fcmpeq */
        case opcode::icmpne:
        {
            std::int32_t a = frame.stack.pop_i32();
            std::int32_t b = frame.stack.pop_i32();
            frame.stack.push_i32(b != a);
            break;
        } /* opcode::icmpne */
        case opcode::fcmpne:
        {
            float a = frame.stack.pop_f32();
            float b = frame.stack.pop_f32();
            frame.stack.push_i32(b != a);
            break;
        } /* opcode::fcmpne */
        case opcode::jnz:
        {
            std::size_t then_offset = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::size_t);
            std::size_t else_offset = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            offset += sizeof(std::size_t);

            std::int32_t cond = frame.stack.pop_i32();
            if(cond != 0)
            {
                offset = then_offset;
            }
            else
            {
                offset = else_offset;
            }
            break;
        } /* opcode::jnz */
        case opcode::jmp:
        {
            offset = *reinterpret_cast<const std::int64_t*>(&binary[offset]);
            break;
        } /* opcode::jmp */
        default:
            throw interpreter_error(fmt::format("Opcode '{}' ({}) not implemented.", to_string(static_cast<opcode>(instr)), instr));
        }
    }

    throw interpreter_error("Out of bounds code read.");
}

value context::exec(const language_module& mod,
                    const function& f,
                    std::vector<value> args)
{
    /*
     * allocate locals and decode arguments.
     */
    stack_frame frame{mod.get_header().strings, f.get_locals_size(), f.get_stack_size()};
    std::vector<std::string> local_strings;

    auto& arg_types = f.get_signature().arg_types;
    if(arg_types.size() != args.size())
    {
        throw interpreter_error(fmt::format("Arguments for function do not match: Expected {}, got {}.", arg_types.size(), args.size()));
    }

    std::size_t offset = 0;
    for(std::size_t i = 0; i < args.size(); ++i)
    {
        if(std::get<0>(arg_types[i]) != std::get<0>(args[i].get_type()))
        {
            throw interpreter_error(
              fmt::format("Argument {} for function has wrong base type (expected '{}', got '{}').",
                          i, std::get<0>(arg_types[i]) != std::get<0>(args[i].get_type())));
        }

        if(std::get<1>(arg_types[i]).has_value())
        {
            if(!std::get<1>(args[i].get_type()).has_value()
               || *std::get<1>(arg_types[i]) != *std::get<1>(args[i].get_type()))
            {
                throw interpreter_error(
                  fmt::format("Argument {} for function has wrong array length (expected '{}', got '{}').",
                              i, *std::get<1>(arg_types[i]) != *std::get<1>(args[i].get_type())));
            }
        }

        if(offset + args[i].get_size() > f.get_locals_size())
        {
            throw interpreter_error("Stack overflow during argument allocation.");
        }

        offset += args[i].write(&frame.locals[offset]);
    }

    /*
     * Execute the function.
     */
    opcode ret_opcode;
    std::int64_t array_length = 0;
    if(f.is_native())
    {
        auto& func = f.get_function();
        func(frame.stack);
        ret_opcode = f.get_return_opcode();
        if(f.returns_array())
        {
            array_length = f.get_returned_array_length();
        }
    }
    else
    {
        auto [oc, length] = exec(mod, f.get_entry_point(), f.get_size(), frame);
        ret_opcode = oc;
        array_length = length;
    }

    /*
     * Decode return value.
     */
    value ret;
    if(ret_opcode == opcode::ret)
    {
        /* return void */
    }
    else if(ret_opcode == opcode::iret)
    {
        if(array_length == 0)
        {
            ret = frame.stack.pop_i32();
        }
        else
        {
            std::vector<int> iret;
            iret.resize(array_length);
            for(std::size_t i = 0; i < array_length; ++i)
            {
                iret[array_length - i - 1] = frame.stack.pop_i32();
            }
            ret = std::move(iret);
        }
    }
    else if(ret_opcode == opcode::fret)
    {
        if(array_length == 0)
        {
            ret = frame.stack.pop_f32();
        }
        else
        {
            std::vector<float> fret;
            fret.resize(array_length);
            for(std::size_t i = 0; i < array_length; ++i)
            {
                fret[array_length - i - 1] = frame.stack.pop_f32();
            }
            ret = std::move(fret);
        }
    }
    else if(ret_opcode == opcode::sret)
    {
        if(array_length == 0)
        {
            ret = std::string{*frame.stack.pop_addr<std::string>()};
        }
        else
        {
            std::vector<std::string> str_ret;
            str_ret.resize(array_length);
            for(std::size_t i = 0; i < array_length; ++i)
            {
                str_ret[array_length - i - 1] = *frame.stack.pop_addr<std::string>();
            }
            ret = std::move(str_ret);
        }
    }
    else
    {
        throw interpreter_error(fmt::format("Invalid return opcode '{}' ({}).", to_string(ret_opcode), static_cast<int>(ret_opcode)));
    }

    return ret;
}

void context::register_native_function(const std::string& mod_name, std::string fn_name, std::function<void(operand_stack&)> func)
{
    auto mod_it = function_map.find(mod_name);
    if(mod_it != function_map.end())
    {
        if(mod_it->second.find(fn_name) != mod_it->second.end())
        {
            throw interpreter_error(fmt::format("Cannot register native function: '{}' is already definded for module '{}'.", fn_name, mod_name));
        }
    }

    auto native_mod_it = native_function_map.find(mod_name);
    if(native_mod_it == native_function_map.end())
    {
        native_function_map.insert({mod_name, {{std::move(fn_name), std::move(func)}}});
    }
    else
    {
        if(native_mod_it->second.find(fn_name) != native_mod_it->second.end())
        {
            throw interpreter_error(fmt::format("Cannot register native function: '{}' is already definded for module '{}'.", fn_name, mod_name));
        }

        native_mod_it->second.insert({std::move(fn_name), std::move(func)});
    }
}

void context::load_module(const std::string& name, const language_module& mod)
{
    if(module_map.find(name) != module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' already loaded.", name));
    }

    module_map.insert({name, decode(mod)});
    module_header& decoded_header = module_map[name]->header;

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
            auto mod_it = native_function_map.find(details.library_name);
            if(mod_it == native_function_map.end())
            {
                // TODO Add e.g. a callback to resolve the library?
                throw interpreter_error(fmt::format("Cannot resolve module '{}' containing native function '{}'.", details.library_name, it.name));
            }

            auto func_it = mod_it->second.find(it.name);
            if(func_it == mod_it->second.end())
            {
                throw interpreter_error(fmt::format("Cannot resolve native function '{}' in module '{}'.", it.name, details.library_name));
            }

            fmap.insert({it.name, function{desc.signature, func_it->second}});
        }
        else
        {
            auto& details = std::get<function_details>(desc.details);
            fmap.insert({it.name, function{desc.signature, details.offset, details.size, details.locals_size, details.stack_size}});
        }
    }
    function_map.insert({name, std::move(fmap)});
}

value context::invoke(const std::string& module_name, const std::string& function_name, std::vector<value> args)
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

    return exec(*mod_it->second, func_it->second, std::move(args));
}

}    // namespace slang::interpreter