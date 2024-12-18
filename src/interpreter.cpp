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
#include "vector.h"

#ifdef INTERPRETER_DEBUG
#    define DEBUG_LOG(...) fmt::print("INT: {}\n", fmt::format(__VA_ARGS__))
#else
#    define DEBUG_LOG(...)
#endif

namespace slang::interpreter
{

namespace ty = slang::typing;

/*
 * Verify size assumptions for arrays.
 */

static_assert(sizeof(fixed_vector<std::int32_t>) == sizeof(void*));
static_assert(sizeof(fixed_vector<float>) == sizeof(void*));
static_assert(sizeof(fixed_vector<std::string*>) == sizeof(void*));
static_assert(sizeof(fixed_vector<void*>) == sizeof(void*));

/**
 * Generate the return opcode from the signature's return type for native functions.
 *
 * @note Only used for preparing handover of return values to native code.
 * @param return_type The return type, given as `(name, is_array)`.
 * @returns A return opcode.
 * @throws Throws an `interpreter_error` if the `return_type` is invalid.
 */
static opcode get_return_opcode(const std::pair<module_::variable_type, bool>& return_type)
{
    auto& name = std::get<0>(return_type);

    if(std::get<1>(return_type))
    {
        return opcode::aret;
    }

    if(name.base_type() == "void")
    {
        return opcode::ret;
    }
    else if(name.base_type() == "i32")
    {
        return opcode::iret;
    }
    else if(name.base_type() == "f32")
    {
        return opcode::fret;
    }
    else if(name.base_type() == "str")
    {
        return opcode::sret;
    }
    else if(name.base_type() == "@addr" || name.base_type() == "@array")
    {
        return opcode::aret;
    }

    // FIXME Assume all other types are references.
    return opcode::aret;
}

/**
 * Check if a type is garbage collected.
 *
 * @param t The type string.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const module_::variable_type& t)
{
    if(t.base_type() == "void")
    {
        throw interpreter_error("Found void type in type info for garbage collector.");
    }

    // check for built-in non-gc types.
    return t.is_array() || ty::is_reference_type(t.base_type());
}

/**
 * Check if a field is garbage collected.
 *
 * @param info The type info.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const slang::module_::field_descriptor& info)
{
    return info.base_type.is_array() || is_garbage_collected(info.base_type);
}

/**
 * Check if a type is garbage collected.
 *
 * @param info The type info, given as a pair `(base_type, is_array)`.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const std::pair<std::string, bool>& info)
{
    return std::get<1>(info) || is_garbage_collected(std::get<0>(info));
}

/*
 * function implementation.
 */

function::function(module_::function_signature signature,
                   std::size_t entry_point,
                   std::size_t size,
                   std::vector<module_::variable_descriptor> locals,
                   std::size_t locals_size,
                   std::size_t stack_size)
: signature{std::move(signature)}
, native{false}
, entry_point_or_function{entry_point}
, size{size}
, locals{std::move(locals)}
, locals_size{locals_size}
, stack_size{stack_size}
{
    ret_opcode = ::slang::interpreter::get_return_opcode(this->signature.return_type);
}

function::function(module_::function_signature signature, std::function<void(operand_stack&)> func)
: signature{std::move(signature)}
, native{true}
, entry_point_or_function{std::move(func)}
{
    ret_opcode = ::slang::interpreter::get_return_opcode(this->signature.return_type);
}

/*
 * context implementation.
 */

/** Byte sizes and alignments for built-in types. */
static const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> type_properties_map = {
  {"void", {0, 0}},
  {"i32", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},
  {"f32", {sizeof(float), std::alignment_of_v<float>}},
  {"str", {sizeof(std::string*), std::alignment_of_v<std::string*>}},
  {"@addr", {sizeof(void*), std::alignment_of_v<void*>}},
  {"@array", {sizeof(void*), std::alignment_of_v<void*>}}};

/** Get the type size (for built-in types) or the size of a type reference (for custom types). */
static std::size_t get_type_or_reference_size(const module_::variable_descriptor& v)
{
    if(v.type.base_type().length() == 0)
    {
        throw interpreter_error("Unable to determine type size for empty type.");
    }

    if(v.type.is_array() || ty::is_reference_type(v.type.base_type()))
    {
        return sizeof(void*);
    }

    auto built_in_it = type_properties_map.find(v.type.base_type());
    if(built_in_it != type_properties_map.end())
    {
        return built_in_it->second.first;
    }

    throw interpreter_error(fmt::format("Unable to determine type size for '{}'.", v.type.base_type()));
}

/** Get the type size (for built-in types) or the size of a type reference (for custom types). */
static std::size_t get_type_or_reference_size(const std::pair<module_::variable_type, bool>& v)
{
    if(std::get<0>(v).base_type().length() == 0)
    {
        throw interpreter_error("Unable to determine type size for empty type.");
    }

    if(std::get<1>(v) || std::get<0>(v).is_array())
    {
        return sizeof(void*);
    }

    auto built_in_it = type_properties_map.find(std::get<0>(v).base_type());
    if(built_in_it != type_properties_map.end())
    {
        return built_in_it->second.first;
    }

    // FIXME Assume all other types are references.
    return sizeof(void*);
}

type_properties context::get_type_properties(
  const std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  const module_::variable_type& type) const
{
    // array types.
    if(type.is_array())
    {
        // FIXME the copy `std::size_t(...)` is here because clang complains about losing `const` qualifier.
        return {sizeof(void*), std::size_t(std::alignment_of_v<void*>), 0};
    }

    // built-in types.
    if(type.base_type() == "void")
    {
        return {0, 0, 0};
    }

    auto built_in_it = type_properties_map.find(type.base_type());
    if(built_in_it != type_properties_map.end())
    {
        return {built_in_it->second.first, built_in_it->second.second, 0};
    }

    // structs.
    auto type_it = struct_map.find(type.base_type());
    if(type_it == struct_map.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve size for type '{}': Type not found.", type.base_type()));
    }

    return {type_it->second.size, type_it->second.alignment, type_it->second.layout_id};
}

field_properties context::get_field_properties(
  const std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  const std::string& type_name,
  std::size_t field_index) const
{
    // built-in types.
    if(type_name == "void")
    {
        throw interpreter_error("Invalid struct type name 'void'.");
    }

    auto built_in_it = type_properties_map.find(type_name);
    if(built_in_it != type_properties_map.end())
    {
        throw interpreter_error(fmt::format("Invalid struct type name '{}'.", type_name));
    }

    // structs.
    auto type_it = struct_map.find(type_name);
    if(type_it == struct_map.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve size for type '{}': Type not found.", type_name));
    }

    auto field_info = type_it->second.member_types.at(field_index);
    return {field_info.second.size, field_info.second.offset, is_garbage_collected(type_name)};
}

std::int32_t context::get_stack_delta(const module_::function_signature& s) const
{
    auto return_type_size = get_type_or_reference_size(s.return_type);
    std::int32_t arg_size = 0;
    for(auto& it: s.arg_types)
    {
        arg_size += get_type_or_reference_size(it);
        // NOTE stack contents are not aligned.
    }
    return return_type_size - arg_size;
}

void context::decode_locals(
  const std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  module_::function_descriptor& desc)
{
    if(desc.native)
    {
        throw interpreter_error("Cannot decode locals for native function.");
    }
    auto& details = std::get<module_::function_details>(desc.details);

    details.args_size = 0;
    details.locals_size = 0;

    std::size_t arg_count = desc.signature.arg_types.size();
    if(arg_count > details.locals.size())
    {
        throw interpreter_error("Function argument count exceeds locals count.");
    }

    // arguments.
    for(std::size_t i = 0; i < arg_count; ++i)
    {
        auto& v = details.locals[i];
        std::size_t size = get_type_or_reference_size(v);

        v.offset = details.args_size;    // NOTE offsets are not aligned.
        v.size = size;

        details.args_size += v.size;
        details.args_size = details.args_size;    // NOTE sizes are not aligned.
    }
    details.locals_size = details.args_size;

    // locals.
    for(std::size_t i = arg_count; i < details.locals.size(); ++i)
    {
        auto& v = details.locals[i];
        std::size_t size = get_type_or_reference_size(v);

        v.offset = details.locals_size;    // NOTE offsets are not aligned.
        v.size = size;

        details.locals_size += v.size;
        details.locals_size = details.locals_size;    // NOTE sizes are not aligned.
    }

    // return type
    details.return_size = get_type_or_reference_size(desc.signature.return_type);
}

std::int32_t context::decode_instruction(
  const std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  module_::language_module& mod,
  archive& ar,
  std::byte instr,
  const module_::function_details& details,
  std::vector<std::byte>& code) const
{
    switch(static_cast<opcode>(instr))
    {
    /* opcodes without arguments. */
    case opcode::aconst_null: [[fallthrough]];
    case opcode::adup:
        return static_cast<std::int32_t>(sizeof(void*));
    case opcode::idup: [[fallthrough]];
    case opcode::fdup:
        return static_cast<std::int32_t>(sizeof(std::int32_t));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::pop:
        return -static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::apop:
        return -static_cast<std::int32_t>(sizeof(void*));
    case opcode::arraylength:
        return -static_cast<std::int32_t>(sizeof(void*)) + static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::iaload: [[fallthrough]];
    case opcode::faload:
        return -static_cast<std::int32_t>(sizeof(void*));
    case opcode::saload:
        return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)) + static_cast<std::int32_t>(sizeof(std::string*));
    case opcode::iastore: [[fallthrough]];
    case opcode::fastore:
        return -static_cast<std::int32_t>(sizeof(void*)) - 2 * static_cast<std::int32_t>(sizeof(std::int32_t));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::sastore:
        return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)) - static_cast<std::int32_t>(sizeof(std::string*));
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
    case opcode::land: [[fallthrough]];
    case opcode::ior: [[fallthrough]];
    case opcode::lor: [[fallthrough]];
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
        return -static_cast<std::int32_t>(sizeof(std::int32_t));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::acmpeq: [[fallthrough]];
    case opcode::acmpne:
        return -2 * static_cast<std::int32_t>(sizeof(void*)) + static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::i2f: [[fallthrough]];
    case opcode::f2i: [[fallthrough]];
    case opcode::ret: [[fallthrough]];
    case opcode::iret: [[fallthrough]];
    case opcode::fret: [[fallthrough]];
    case opcode::sret: [[fallthrough]];
    case opcode::aret:
        return 0;
    /* opcodes with one 1-byte argument. */
    case opcode::newarray:
    {
        std::uint8_t i_u8;
        ar & i_u8;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u8), reinterpret_cast<std::byte*>(&i_u8) + sizeof(i_u8));
        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32;
        ar & i_u32;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i_u32), reinterpret_cast<std::byte*>(&i_u32) + sizeof(i_u32));
        return static_cast<std::int32_t>(sizeof(std::uint32_t));
    }
    /* opcodes with one VLE integer. */
    case opcode::sconst:
    {
        vle_int i;
        ar & i;

        code.insert(code.end(), reinterpret_cast<std::byte*>(&i.i), reinterpret_cast<std::byte*>(&i.i) + sizeof(i.i));
        return static_cast<std::int32_t>(sizeof(std::string*));
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
        return -static_cast<std::int32_t>(sizeof(std::int32_t));
    }
    /* dup_x1. */
    case opcode::dup_x1:
    {
        // type arguments.
        module_::variable_type t1, t2;
        ar & t1 & t2;

        // "void" is not allowed.
        if(t1.base_type() == "void" || t2.base_type() == "void")
        {
            throw interpreter_error("Error decoding dup_x1 instruction: Invalid argument type 'void'.");
        }

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto properties1 = get_type_properties({}, t1);
        auto properties2 = get_type_properties({}, t2);

        // check if the type needs garbage collection.
        std::uint8_t needs_gc = is_garbage_collected(t1);

        code.insert(code.end(), reinterpret_cast<std::byte*>(&properties1.size), reinterpret_cast<std::byte*>(&properties1.size) + sizeof(properties1.size));
        code.insert(code.end(), reinterpret_cast<std::byte*>(&properties2.size), reinterpret_cast<std::byte*>(&properties2.size) + sizeof(properties2.size));
        code.insert(code.end(), reinterpret_cast<std::byte*>(&needs_gc), reinterpret_cast<std::byte*>(&needs_gc) + sizeof(needs_gc));
        return static_cast<std::int32_t>(properties1.size);
    }
    /* invoke. */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        // resolve to address.
        if(i.i < 0)
        {
            std::int64_t import_index = -i.i - 1;    // this is bounded by 0 from below by the `if` check.
            if(static_cast<std::size_t>(import_index) >= mod.header.imports.size())
            {
                throw interpreter_error(fmt::format("Import index {} out of range ({} >= {}).", import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::function)
            {
                throw interpreter_error(fmt::format("Cannot resolve call: Import header at index {} does not refer to a function.", -i.i - 1));
            }

            auto& exp_symbol = std::get<module_::exported_symbol*>(imp_symbol.export_reference);
            if(exp_symbol->type != imp_symbol.type)
            {
                throw interpreter_error(
                  fmt::format(
                    "Cannot resolve call: Export type for '{}' does not match import type ({} != {}).",
                    imp_symbol.name,
                    slang::module_::to_string(exp_symbol->type),
                    slang::module_::to_string(imp_symbol.type)));
            }

            auto& desc = std::get<module_::function_descriptor>(exp_symbol->desc);

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(
                  fmt::format("Package import index {} out of range ({} >= {}).",
                              imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            const module_::language_module* mod_ptr =
              std::get<const module_::language_module*>(mod.header.imports[imp_symbol.package_index].export_reference);
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&mod_ptr), reinterpret_cast<const std::byte*>(&mod_ptr) + sizeof(mod_ptr));

            const module_::function_descriptor* desc_ptr = &desc;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&desc_ptr), reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));

            if(mod_ptr == nullptr)
            {
                throw interpreter_error(fmt::format("Unresolved module import '{}'.", mod.header.imports[imp_symbol.package_index].name));
            }

            return get_stack_delta(desc.signature);
        }
        else
        {
            if(static_cast<std::size_t>(i.i) >= mod.header.exports.size())
            {
                throw interpreter_error(fmt::format("Export index {} out of range ({} >= {}).", i.i, i.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[i.i];
            if(exp_symbol.type != module_::symbol_type::function)
            {
                throw interpreter_error(fmt::format("Cannot resolve call: Header entry at index {} is not a function.", i.i));
            }
            auto& desc = std::get<module_::function_descriptor>(exp_symbol.desc);

            const module_::language_module* mod_ptr = &mod;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&mod_ptr), reinterpret_cast<const std::byte*>(&mod_ptr) + sizeof(mod_ptr));

            const module_::function_descriptor* desc_ptr = &desc;
            code.insert(code.end(), reinterpret_cast<const std::byte*>(&desc_ptr), reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));

            if(desc.native && !static_cast<bool>(std::get<1>(desc.details).func))
            {
                throw interpreter_error("Native function was null during decode.");
            }

            return get_stack_delta(desc.signature);
        }
    }
    /* opcodes that need to resolve a variable. */
    case opcode::iload: [[fallthrough]];
    case opcode::fload: [[fallthrough]];
    case opcode::aload: [[fallthrough]];
    case opcode::istore: [[fallthrough]];
    case opcode::fstore: [[fallthrough]];
    case opcode::astore:
    {
        vle_int i;
        ar & i;

        if(i.i < 0 || static_cast<std::size_t>(i.i) >= details.locals.size())
        {
            throw interpreter_error(fmt::format("Index '{}' for argument or local outside of valid range 0-{}.", i.i, details.locals.size()));
        }

        std::int64_t offset = details.locals[i.i].offset;
        code.insert(code.end(), reinterpret_cast<std::byte*>(&offset), reinterpret_cast<std::byte*>(&offset) + sizeof(offset));

        // return correct size.
        bool is_store = (static_cast<opcode>(instr) == opcode::istore)
                        || (static_cast<opcode>(instr) == opcode::fstore)
                        || (static_cast<opcode>(instr) == opcode::astore);
        bool is_ref = (static_cast<opcode>(instr) == opcode::aload)
                      || (static_cast<opcode>(instr) == opcode::astore);

        if(is_ref)
        {
            return is_store ? -static_cast<std::int32_t>(sizeof(void*)) : sizeof(void*);
        }
        else
        {
            return is_store ? -static_cast<std::int32_t>(sizeof(std::uint32_t)) : sizeof(std::uint32_t);    // same size for i32/f32 (since sizeof(float) == sizeof(std::uint32_t))
        }
    }
    /* new. */
    case opcode::new_:
    {
        vle_int i;
        ar & i;

        if(i.i < 0)
        {
            // imported type.
            std::size_t import_index = -i.i - 1;
            if(static_cast<std::size_t>(import_index) >= mod.header.imports.size())
            {
                throw interpreter_error(fmt::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(fmt::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(fmt::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(fmt::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            auto mod_it = module_map.find(imp_package.name);
            if(mod_it == module_map.end())
            {
                throw interpreter_error(fmt::format(
                  "Referenced module '{}' not loaded.",
                  imp_package.name));
            }

            auto struct_map_it = this->struct_map.find(imp_package.name);
            if(struct_map_it == this->struct_map.end())
            {
                throw interpreter_error(fmt::format(
                  "Type map for module '{}' not loaded.",
                  imp_package.name));
            }

            auto properties = get_type_properties(struct_map_it->second, imp_symbol.name);
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.size), reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.alignment), reinterpret_cast<std::byte*>(&properties.alignment) + sizeof(properties.alignment));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.layout_id), reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));
        }
        else
        {
            // exported type.

            if(static_cast<std::size_t>(i.i) >= mod.header.exports.size())
            {
                throw interpreter_error(fmt::format("Export index {} out of range ({} >= {}).",
                                                    i.i, i.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[i.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(fmt::format(
                  "Cannot resolve type: Export header entry at index {} is not a type.",
                  i.i));
            }

            auto properties = get_type_properties(struct_map, exp_symbol.name);
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.size), reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.alignment), reinterpret_cast<std::byte*>(&properties.alignment) + sizeof(properties.alignment));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.layout_id), reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));
        }

        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* setfield, getfield. */
    case opcode::setfield: [[fallthrough]];
    case opcode::getfield:
    {
        vle_int struct_index, field_index;
        ar & struct_index & field_index;

        field_properties properties;

        if(struct_index.i < 0)
        {
            // imported type.
            std::size_t import_index = -struct_index.i - 1;
            if(static_cast<std::size_t>(import_index) >= mod.header.imports.size())
            {
                throw interpreter_error(fmt::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(fmt::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(fmt::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(fmt::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            auto mod_it = module_map.find(imp_package.name);
            if(mod_it == module_map.end())
            {
                throw interpreter_error(fmt::format(
                  "Referenced module '{}' not loaded.",
                  imp_package.name));
            }

            auto struct_map_it = this->struct_map.find(imp_package.name);
            if(struct_map_it == this->struct_map.end())
            {
                throw interpreter_error(fmt::format(
                  "Type map for module '{}' not loaded.",
                  imp_package.name));
            }

            properties = get_field_properties(struct_map_it->second, imp_symbol.name, field_index.i);
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.size), reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.offset), reinterpret_cast<std::byte*>(&properties.offset) + sizeof(properties.offset));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.needs_gc), reinterpret_cast<std::byte*>(&properties.needs_gc) + sizeof(properties.needs_gc));
        }
        else
        {
            if(static_cast<std::size_t>(struct_index.i) >= mod.header.exports.size())
            {
                throw interpreter_error(fmt::format("Export index {} out of range ({} >= {}).", struct_index.i, struct_index.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[struct_index.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(fmt::format("Cannot resolve type: Header entry at index {} is not a type.", struct_index.i));
            }

            properties = get_field_properties(struct_map, exp_symbol.name, field_index.i);
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.size), reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.offset), reinterpret_cast<std::byte*>(&properties.offset) + sizeof(properties.offset));
            code.insert(code.end(), reinterpret_cast<std::byte*>(&properties.needs_gc), reinterpret_cast<std::byte*>(&properties.needs_gc) + sizeof(properties.needs_gc));
        }

        if(static_cast<opcode>(instr) == opcode::setfield)
        {
            return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(properties.size);
        }
        else
        {
            return -static_cast<std::int32_t>(sizeof(void*)) + static_cast<std::int32_t>(properties.size);
        }
    }
    default:
        throw interpreter_error(fmt::format("Unexpected opcode '{}' ({}) during decode.", to_string(static_cast<opcode>(instr)), static_cast<int>(instr)));
    }
}

std::function<void(operand_stack&)> context::resolve_native_function(const std::string& name, const std::string& library_name) const
{
    auto mod_it = native_function_map.find(library_name);
    if(mod_it == native_function_map.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (library not found).", name, library_name));
    }

    auto func_it = mod_it->second.find(name);
    if(func_it == mod_it->second.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (function not found).", name, library_name));
    }

    DEBUG_LOG("resolved imported native function '{}.{}'.", library_name, name);

    return func_it->second;
}

void context::decode_structs(
  std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  const module_::language_module& mod)
{
    for(auto& [name, desc]: struct_map)
    {
        std::size_t size = 0;
        std::size_t alignment = 0;
        std::vector<std::size_t> layout;
        int offset = 0;

        for(auto& [member_name, member_type]: desc.member_types)
        {
            bool add_to_layout = false;

            // check that the type exists and get its properties.
            auto built_in_it = type_properties_map.find(member_type.base_type.base_type());
            if(built_in_it != type_properties_map.end())
            {
                if(is_garbage_collected(member_type))
                {
                    member_type.size = sizeof(void*);
                    member_type.alignment = std::alignment_of_v<void*>;

                    add_to_layout = true;
                }
                else
                {
                    member_type.size = built_in_it->second.first;
                    member_type.alignment = built_in_it->second.second;
                }
            }
            else
            {
                if(struct_map.find(member_type.base_type.base_type()) == struct_map.end())
                {
                    throw interpreter_error(
                      fmt::format("Cannot resolve size for type '{}': Type not found.",
                                  member_type.base_type.base_type()));
                }

                // size and alignment are the same for both array and non-array types.
                member_type.size = sizeof(void*);
                member_type.alignment = std::alignment_of_v<void*>;

                add_to_layout = true;
            }

            // store offset.
            member_type.offset = (static_cast<std::size_t>(offset) + (member_type.alignment - 1)) & ~(member_type.alignment - 1);

            // calculate member offset as `size_after - size_before`.
            offset -= size;

            // update struct size and alignment.
            size += member_type.size;
            size = (size + (member_type.alignment - 1)) & ~(member_type.alignment - 1);

            alignment = std::max(alignment, member_type.alignment);

            // calculate member offset as `size_after - size_before`.
            offset += size;

            // update type layout.
            if(add_to_layout)
            {
                layout.push_back(member_type.offset);
            }
        }

        // trailing padding.
        size = (size + (alignment - 1)) & ~(alignment - 1);

        // store type size and alignment.
        desc.size = size;
        desc.alignment = alignment;

        // store layout.
        desc.layout_id = gc.register_type_layout(std::move(layout));
    }
}

std::unique_ptr<module_::language_module> context::decode(
  const std::unordered_map<std::string, module_::struct_descriptor>& struct_map,
  const module_::language_module& mod)
{
    if(mod.is_decoded())
    {
        throw interpreter_error("Tried to decode a module that already is decoded.");
    }

    module_::module_header header = mod.get_header();
    memory_read_archive ar{mod.get_binary(), true, slang::endian::little};

    /*
     * resolve imports.
     */
    for(auto& it: header.imports)
    {
        if(it.type == module_::symbol_type::package)
        {
            // packages are loaded while resolving other symbols.
            continue;
        }

        // resolve the symbol's package.
        if(it.package_index >= header.imports.size())
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' has invalid package index.", it.name));
        }
        else if(header.imports[it.package_index].type != module_::symbol_type::package)
        {
            throw interpreter_error(fmt::format("Error while resolving imports: Import symbol '{}' refers to non-package import entry.", it.name));
        }

        std::string import_name = header.imports[it.package_index].name;
        slang::utils::replace_all(import_name, package::delimiter, "/");

        fs::path fs_path = fs::path{import_name}.replace_extension(package::module_ext);
        fs::path resolved_path = file_mgr.resolve(fs_path);
        std::unique_ptr<slang::file_archive> ar = file_mgr.open(resolved_path, slang::file_manager::open_mode::read);

        module_::language_module import_mod;
        (*ar) & import_mod;

        if(module_map.find(import_name) == module_map.end())
        {
            load_module(import_name, import_mod);
            header.imports[it.package_index].export_reference = module_map[import_name].get();
        }

        // find the imported symbol.
        module_::module_header& import_header = module_map[import_name]->header;
        auto exp_it = std::find_if(import_header.exports.begin(), import_header.exports.end(),
                                   [&it](const module_::exported_symbol& exp) -> bool
                                   {
                                       return exp.name == it.name;
                                   });
        if(exp_it == import_header.exports.end())
        {
            throw interpreter_error(
              fmt::format(
                "Error while resolving imports: Symbol '{}' is not exported by module '{}'.",
                it.name, import_name));
        }
        if(exp_it->type != it.type)
        {
            throw interpreter_error(
              fmt::format(
                "Error while resolving imports: Symbol '{}' from module '{}' has wrong type (expected '{}', got '{}').",
                it.name,
                import_name,
                slang::module_::to_string(it.type),
                slang::module_::to_string(exp_it->type)));
        }

        it.export_reference = &(*exp_it);

        // resolve symbol
        if(it.type != module_::symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<module_::function_descriptor>(exp_it->desc);
        if(desc.native)
        {
            // resolve native function.
            auto& details = std::get<module_::native_function_details>(desc.details);
            details.func = resolve_native_function(exp_it->name, details.library_name);
        }
    }

    /*
     * arguments and locals.
     */
    for(auto& it: header.exports)
    {
        if(it.type != module_::symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<module_::function_descriptor>(it.desc);
        if(desc.native)
        {
            // resolve native function.
            auto& details = std::get<module_::native_function_details>(desc.details);
            details.func = resolve_native_function(it.name, details.library_name);
            continue;
        }

        decode_locals(struct_map, desc);
    }

    // store header in decoded module.
    auto decoded_module = std::make_unique<module_::language_module>(std::move(header));

    /*
     * instructions.
     */
    std::vector<std::byte> code;
    for(auto& it: decoded_module->header.exports)
    {
        if(it.type != module_::symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<module_::function_descriptor>(it.desc);
        if(desc.native)
        {
            continue;
        }

        auto& details = std::get<module_::function_details>(desc.details);

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

            stack_size += decode_instruction(struct_map, *decoded_module, ar, instr, details, code);
            if(stack_size < 0)
            {
                throw interpreter_error("Error during decode: Got negative stack size.");
            }

            if(static_cast<std::size_t>(stack_size) > max_stack_size)
            {
                max_stack_size = stack_size;
            }
        }

        details.size = code.size() - details.offset;
        details.stack_size = max_stack_size;

        // jump target resolution.
        for(auto& [origin, id]: decoded_module->jump_origins)
        {
            auto target_it = decoded_module->jump_targets.find(id);
            if(target_it == decoded_module->jump_targets.end())
            {
                throw interpreter_error(fmt::format("Unable to resolve jump target for label '{}'.", id));
            }
            std::memcpy(&code[origin], &target_it->second, sizeof(std::size_t));
        }
    }

    decoded_module->set_binary(std::move(code));
    decoded_module->decoded = true;

    return decoded_module;
}

/** Handle local variable construction and destruction. */
class locals_scope
{
    /** The associated interpreter context. */
    context& ctx;

    /** Locals. */
    const std::vector<module_::variable_descriptor>& locals;

    /** The function's stack frame. */
    stack_frame& frame;

    /** Whether to destruct the locals. */
    bool needs_destruct{true};

public:
    /**
     * Create a scope for creating/destroying variables. If needed, adds the locals as
     * roots to the garbage collector.
     *
     * @param ctx The interpreter context.
     * @param locals The locals.
     * @param frame The stack frame.
     */
    locals_scope(context& ctx, const std::vector<module_::variable_descriptor>& locals, stack_frame& frame)
    : ctx{ctx}
    , locals{locals}
    , frame{frame}
    {
        for(auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;
                std::memcpy(&addr, &frame.locals[local.offset], sizeof(void*));
                if(addr != nullptr)
                {
                    ctx.get_gc().add_root(addr);
                    // FIXME We (likely) want to remove the temporaries from the GC here,
                    //       instead of at the caller (see comment there).
                }
            }
        }
    }

    /** Destructor. Calls `destruct`, if needed. */
    ~locals_scope()
    {
        if(needs_destruct)
        {
            destruct();
        }
    }

    /**
     * Remove locals from the garbage collector, if needed.
     *
     * @throws Throws `interpreter_error` if the caller tried to destruct the locals more than once.
     */
    void destruct()
    {
        if(!needs_destruct)
        {
            throw interpreter_error("Local destructors called multiple times.");
        }
        needs_destruct = false;

        for(auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;
                std::memcpy(&addr, &frame.locals[local.offset], sizeof(void*));
                if(addr != nullptr)
                {
                    ctx.get_gc().remove_root(addr);
                }
            }
        }
    }
};

/**
 * Read a value from the binary without bounds check.
 *
 * @param binary The binary data.
 * @param offset The offset into the binary to read from. Holds the new offset after the read.
 * @returns Returns the read value.
 */
template<typename T>
static T read_unchecked(const std::vector<std::byte>& binary, std::size_t& offset)
{
    T v;
    std::memcpy(&v, &binary[offset], sizeof(T));
    offset += sizeof(T);
    return v;
}

opcode context::exec(
  const module_::language_module& mod,
  std::size_t entry_point,
  std::size_t size,
  const std::vector<module_::variable_descriptor>& locals,
  stack_frame& frame)
{
    std::size_t offset = entry_point;

    try
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
        if(offset >= binary.size())
        {
            throw interpreter_error(fmt::format("Entry point is outside the loaded code segment ({} >= {}).", offset, binary.size()));
        }

        locals_scope ls{*this, locals, frame};

        const std::size_t function_end = offset + size;
        while(offset < function_end)
        {
            auto instr = binary[offset];
            ++offset;

            // return.
            if(instr >= static_cast<std::byte>(opcode::ret) && instr <= static_cast<std::byte>(opcode::aret))
            {
                // NOTE The stack size is validated by the caller.

                // destruct the locals here for the GC to clean them up.
                ls.destruct();

                // run garbage collector.
                gc.run();

                --call_stack_level;
                return static_cast<opcode>(instr);
            }

            if(offset == function_end)
            {
                throw interpreter_error(fmt::format("Execution reached function boundary."));
            }

            switch(static_cast<opcode>(instr))
            {
            case opcode::idup: [[fallthrough]];
            case opcode::fdup:
            {
                frame.stack.dup_i32();
                break;
            } /* opcode::idup, opcode::fdup */
            case opcode::adup:
            {
                frame.stack.dup_addr();
                void* addr;
                std::memcpy(&addr, frame.stack.end(sizeof(void*)), sizeof(void*));
                gc.add_temporary(addr);
                break;
            } /* opcode::adup */
            case opcode::dup_x1:
            {
                std::size_t size1 = read_unchecked<std::size_t>(binary, offset);
                std::size_t size2 = read_unchecked<std::size_t>(binary, offset);
                std::uint8_t needs_gc = read_unchecked<std::uint8_t>(binary, offset);
                frame.stack.dup_x1(size1, size2);

                if(needs_gc)
                {
                    void* addr;
                    std::memcpy(&addr, frame.stack.end(2 * size1 + size2), size1);
                    gc.add_temporary(addr);
                }
                break;
            } /* opcode::dup_x1 */
            case opcode::pop:
            {
                frame.stack.pop_i32();
                break;
            } /* opcode::pop */
            case opcode::apop:
            {
                gc.remove_temporary(frame.stack.pop_addr<void>());
                break;
            } /* opcode::apop */
            case opcode::iadd:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i + v; });
                break;
            } /* opcode::iadd */
            case opcode::isub:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i - v; });
                break;
            } /* opcode::isub */
            case opcode::imul:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i * v; });
                break;
            } /* opcode::imul */
            case opcode::idiv:
            {
                std::int32_t divisor = frame.stack.pop_i32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<std::int32_t, std::int32_t>([divisor](std::int32_t dividend) -> std::int32_t
                                                                   { return dividend / divisor; });
                break;
            } /* opcode::idiv */
            case opcode::imod:
            {
                std::int32_t divisor = frame.stack.pop_i32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<std::int32_t, std::int32_t>([divisor](std::int32_t dividend) -> std::int32_t
                                                                   { return dividend % divisor; });
                break;
            } /* opcode::imod */
            case opcode::fadd:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([v](float i) -> float
                                                     { return i + v; });
                break;
            } /* opcode::fadd */
            case opcode::fsub:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([v](float i) -> float
                                                     { return i - v; });
                break;
            } /* opcode::fsub */
            case opcode::fmul:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([v](float i) -> float
                                                     { return i * v; });
                break;
            } /* opcode::fmul */
            case opcode::fdiv:
            {
                float divisor = frame.stack.pop_f32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<float, float>([divisor](float dividend) -> float
                                                     { return dividend / divisor; });
                break;
            } /* opcode::fdiv */
            case opcode::i2f:
            {
                frame.stack.modify_top<std::int32_t, float>([](const std::int32_t i) -> float
                                                            { return static_cast<float>(i); });
                break;
            } /* opcode::i2f */
            case opcode::f2i:
            {
                frame.stack.modify_top<float, std::int32_t>([](const float f) -> std::int32_t
                                                            { return static_cast<std::int32_t>(f); });
                break;
            } /* opcode::f2i */
            case opcode::aconst_null:
            {
                frame.stack.push_addr<void>(nullptr);
                break;
            } /* opcode::aconst_null */
            case opcode::iconst: [[fallthrough]];
            case opcode::fconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::uint32_t i_u32 = read_unchecked<std::uint32_t>(binary, offset);
                frame.stack.push_i32(i_u32);
                break;
            } /* opcode::iconst, opcode::fconst */
            case opcode::sconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);
                if(i < 0 || static_cast<std::size_t>(i) >= frame.string_table.size())
                {
                    throw interpreter_error(fmt::format("Invalid index '{}' into string table.", i));
                }

                auto str = gc.gc_new<std::string>(gc::gc_object::of_temporary);
                *str = frame.string_table[i];

                frame.stack.push_addr(str);
                break;
            } /* opcode::sconst */
            case opcode::iaload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                fixed_vector<std::int32_t>* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                frame.stack.push_i32((*arr)[array_index]);
                break;
            } /* opcode::iaload */
            case opcode::faload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                fixed_vector<float>* arr = frame.stack.pop_addr<fixed_vector<float>>();

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                frame.stack.push_f32((*arr)[array_index]);
                break;
            } /* opcode::faload */
            case opcode::saload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                fixed_vector<std::string*>* arr = frame.stack.pop_addr<fixed_vector<std::string*>>();

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                void* obj = (*arr)[array_index];
                gc.add_temporary(obj);

                frame.stack.push_addr(obj);
                break;
            } /* opcode::saload */
            case opcode::iastore:
            {
                std::int32_t v = frame.stack.pop_i32();
                std::int32_t index = frame.stack.pop_i32();
                fixed_vector<std::int32_t>* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = v;
                break;
            } /* opcode::iastore */
            case opcode::fastore:
            {
                float v = frame.stack.pop_f32();
                std::int32_t index = frame.stack.pop_i32();
                fixed_vector<float>* arr = frame.stack.pop_addr<fixed_vector<float>>();

                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = v;
                break;
            } /* opcode::fastore */
            case opcode::sastore:
            {
                std::string* s = frame.stack.pop_addr<std::string>();
                std::int32_t index = frame.stack.pop_i32();
                fixed_vector<std::string*>* arr = frame.stack.pop_addr<fixed_vector<std::string*>>();

                gc.remove_temporary(s);
                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = s;

                break;
            } /* opcode::sastore */
            case opcode::iload: [[fallthrough]];
            case opcode::fload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::uint32_t) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                std::uint32_t v;
                std::memcpy(&v, &frame.locals[i], sizeof(v));
                frame.stack.push_i32(v);
                break;
            } /* opcode::iload, opcode::fload */
            case opcode::aload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(void*) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                void* addr;
                std::memcpy(&addr, &frame.locals[i], sizeof(addr));
                gc.add_temporary(addr);
                frame.stack.push_addr(addr);
                break;
            } /* opcode::aload */
            case opcode::istore: [[fallthrough]];
            case opcode::fstore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::uint32_t) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                std::int32_t v = frame.stack.pop_i32();
                std::memcpy(&frame.locals[i], &v, sizeof(v));
                break;
            } /* opcode::istore, opcode::fstore */
            case opcode::astore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::string*) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                auto obj = frame.stack.pop_addr<void>();
                gc.remove_temporary(obj);

                void* prev;
                std::memcpy(&prev, &frame.locals[i], sizeof(prev));
                if(obj != prev)
                {
                    if(prev != nullptr)
                    {
                        gc.remove_root(prev);
                    }
                    if(obj != nullptr)
                    {
                        gc.add_root(obj);
                    }
                }

                std::memcpy(&frame.locals[i], &obj, sizeof(obj));
                break;
            } /* opcode::astore */
            case opcode::invoke:
            {
                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_::language_module const* callee_mod;
                std::memcpy(&callee_mod, &binary[offset], sizeof(callee_mod));
                offset += sizeof(callee_mod);

                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_::function_descriptor const* desc;
                std::memcpy(&desc, &binary[offset], sizeof(desc));
                offset += sizeof(desc);

                if(desc->native)
                {
                    auto& details = std::get<module_::native_function_details>(desc->details);
                    if(!details.func)
                    {
                        throw interpreter_error("Tried to invoke unresolved native function.");
                    }

                    details.func(frame.stack);
                }
                else
                {
                    auto& details = std::get<module_::function_details>(desc->details);

                    // prepare stack frame
                    stack_frame callee_frame{frame.string_table, details.locals_size, details.stack_size};

                    auto* args_start = reinterpret_cast<std::byte*>(frame.stack.end(details.args_size));
                    std::copy(args_start, args_start + details.args_size, callee_frame.locals.data());
                    frame.stack.discard(details.args_size);

                    // clean up arguments in GC
                    // FIXME This (likely) should be done by the callee, after making them roots.
                    for(std::size_t i = 0; i < desc->signature.arg_types.size(); ++i)
                    {
                        auto& arg = details.locals[i];

                        if(arg.type.is_array() || arg.reference)
                        {
                            void* addr;
                            std::memcpy(&addr, &callee_frame.locals[arg.offset], sizeof(addr));
                            gc.remove_temporary(addr);
                        }
                    }

                    // invoke function
                    exec(*callee_mod, details.offset, details.size, details.locals, callee_frame);

                    // store return value
                    if(callee_frame.stack.size() != details.return_size)
                    {
                        throw interpreter_error(fmt::format("Expected {} bytes to be returned from function call, got {}.", details.return_size, callee_frame.stack.size()));
                    }
                    frame.stack.push_stack(callee_frame.stack);
                }
                break;
            } /* opcode::invoke */
            case opcode::new_:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t size = read_unchecked<std::size_t>(binary, offset);
                std::size_t alignment = read_unchecked<std::size_t>(binary, offset);
                std::size_t layout_id = read_unchecked<std::size_t>(binary, offset);

                frame.stack.push_addr(gc.gc_new(layout_id, size, alignment, gc::gc_object::of_temporary));

                break;
            } /* opcode::new_ */
            case opcode::newarray:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::uint8_t type = read_unchecked<std::uint8_t>(binary, offset);

                std::int32_t size = frame.stack.pop_i32();
                if(size < 0)
                {
                    throw interpreter_error(fmt::format("Invalid array size '{}'.", size));
                }

                if(type == static_cast<std::uint8_t>(module_::array_type::i32))
                {
                    frame.stack.push_addr(gc.gc_new_array<std::int32_t>(size, gc::gc_object::of_temporary));
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::f32))
                {
                    frame.stack.push_addr(gc.gc_new_array<float>(size, gc::gc_object::of_temporary));
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::str))
                {
                    auto array = gc.gc_new_array<std::string*>(size, gc::gc_object::of_temporary);
                    for(std::string*& s: *array)
                    {
                        s = gc.gc_new<std::string>(gc::gc_object::of_none, false);
                    }
                    frame.stack.push_addr(array);
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::ref))
                {
                    frame.stack.push_addr(gc.gc_new_array<void*>(size, gc::gc_object::of_temporary));
                }
                else
                {
                    throw interpreter_error(fmt::format("Unkown array type '{}' for newarray.", type));
                }

                break;
            } /* opcode::newarray */
            case opcode::arraylength:
            {
                // convert to any fixed_vector type.
                auto v = frame.stack.pop_addr<fixed_vector<void*>>();
                if(v == nullptr)
                {
                    throw interpreter_error("Null pointer access during arraylength.");
                }
                gc.remove_temporary(v);
                frame.stack.push_i32(v->size());
                break;
            } /* opcode::arraylength */
            case opcode::setfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t field_size = read_unchecked<std::size_t>(binary, offset);
                std::size_t field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                if(field_size == sizeof(std::int32_t))
                {
                    std::int32_t v = frame.stack.pop_i32();
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    std::memcpy(reinterpret_cast<std::byte*>(type_ref) + field_offset, &v, sizeof(v));
                }
                else if(field_size == sizeof(void*))
                {
                    void* v = frame.stack.pop_addr<void>();
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    if(field_needs_gc)
                    {
                        gc.remove_temporary(v);
                    }

                    std::memcpy(reinterpret_cast<std::byte*>(type_ref) + field_offset, &v, sizeof(v));
                }
                else
                {
                    throw interpreter_error(fmt::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::setfield */
            case opcode::getfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t field_size = read_unchecked<std::size_t>(binary, offset);
                std::size_t field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                if(field_size == sizeof(std::int32_t))
                {
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    std::int32_t v;
                    std::memcpy(&v, reinterpret_cast<std::byte*>(type_ref) + field_offset, sizeof(v));
                    frame.stack.push_i32(v);
                }
                else if(field_size == sizeof(void*))
                {
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    void* v;
                    std::memcpy(&v, reinterpret_cast<std::byte*>(type_ref) + field_offset, sizeof(v));
                    frame.stack.push_addr(v);

                    if(field_needs_gc)
                    {
                        gc.add_temporary(v);
                    }
                }
                else
                {
                    throw interpreter_error(fmt::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::getfield */
            case opcode::iand:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i & v; });
                break;
            } /* opcode::iand */
            case opcode::land:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return (i != 0) && (v != 0); });
                break;
            } /* opcode::land */
            case opcode::ior:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i | v; });
                break;
            } /* opcode::ior */
            case opcode::lor:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return (i != 0) || (v != 0); });
                break;
            } /* opcode::lor */
            case opcode::ixor:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([v](std::int32_t i) -> std::int32_t
                                                                   { return i ^ v; });
                break;
            } /* opcode::ixor */
            case opcode::ishl:
            {
                std::int32_t a = frame.stack.pop_i32();
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>([a_u32](std::int32_t s) -> std::int32_t
                                                                   { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s); 
                                   return s_u32 << a_u32; });
                break;
            } /* opcode::ishl */
            case opcode::ishr:
            {
                std::int32_t a = frame.stack.pop_i32();
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>([a_u32](std::int32_t s) -> std::int32_t
                                                                   { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);
                                   return s_u32 >> a_u32; });
                break;
            } /* opcode::ishr */
            case opcode::icmpl:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b < a); });
                break;
            } /* opcode::icmpl */
            case opcode::fcmpl:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b < a); });
                break;
            } /* opcode::fcmpl */
            case opcode::icmple:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b <= a); });
                break;
            } /* opcode::icmple */
            case opcode::fcmple:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b <= a); });
                break;
            } /* opcode::fcmple */
            case opcode::icmpg:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b > a); });
                break;
            } /* opcode::icmpg */
            case opcode::fcmpg:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b > a); });
                break;
            } /* opcode::fcmpg */
            case opcode::icmpge:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b >= a); });
                break;
            } /* opcode::icmpge */
            case opcode::fcmpge:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b >= a); });
                break;
            } /* opcode::fcmpge */
            case opcode::icmpeq:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b == a); });
                break;
            } /* opcode::icmpeq */
            case opcode::fcmpeq:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b == a); });
                break;
            } /* opcode::fcmpeq */
            case opcode::icmpne:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>([a](std::int32_t b) -> std::int32_t
                                                                   { return (b != a); });
                break;
            } /* opcode::icmpne */
            case opcode::fcmpne:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>([a](float b) -> float
                                                     { return (b != a); });
                break;
            } /* opcode::fcmpne */
            case opcode::acmpeq:
            {
                void* a = frame.stack.pop_addr<void>();
                void* b = frame.stack.pop_addr<void>();
                gc.remove_temporary(a);
                gc.remove_temporary(b);
                frame.stack.push_i32(b == a);
                break;
            } /* opcode::acmpeq */
            case opcode::acmpne:
            {
                void* a = frame.stack.pop_addr<void>();
                void* b = frame.stack.pop_addr<void>();
                gc.remove_temporary(a);
                gc.remove_temporary(b);
                frame.stack.push_i32(b != a);
                break;
            } /* opcode::acmpne */
            case opcode::jnz:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t then_offset = read_unchecked<std::size_t>(binary, offset);
                std::size_t else_offset = read_unchecked<std::size_t>(binary, offset);

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
                /* no out-of-bounds read possible, since this is checked during decode. */
                offset = read_unchecked<std::int64_t>(binary, offset);
                break;
            } /* opcode::jmp */
            default:
                throw interpreter_error(fmt::format("Opcode '{}' ({}) not implemented.", to_string(static_cast<opcode>(instr)), instr));
            }
        }

        throw interpreter_error("Out of bounds code read.");
    }
    catch(interpreter_error& e)
    {
        stack_trace_handler(e, mod, entry_point, offset);
        throw e;
    }
    catch(gc::gc_error& e)
    {
        interpreter_error e_int{e.what()};
        stack_trace_handler(e_int, mod, entry_point, offset);
        throw e_int;
    }
}

void context::stack_trace_handler(interpreter_error& err,
                                  const module_::language_module& mod,
                                  std::size_t entry_point,
                                  std::size_t offset)
{
    // find the module name.
    for(auto& [mod_name, mod_it]: module_map)
    {
        if(mod_it.get() == &mod)
        {
            err.add_stack_trace_entry(mod_name, entry_point, offset);
            return;
        }
    }

    err.add_stack_trace_entry("<unknown>", entry_point, offset);
}

std::string context::stack_trace_to_string(const std::vector<stack_trace_entry>& stack_trace)
{
    std::string buf;
    for(auto& entry: stack_trace)
    {
        // resolve the offset to a function name.
        std::string func_name = fmt::format("<unknown at {}>", entry.entry_point);

        auto mod_it = function_map.find(entry.mod_name);
        if(mod_it != function_map.end())
        {
            auto func_it = std::find_if(mod_it->second.cbegin(), mod_it->second.cend(),
                                        [&entry](const auto& f) -> bool
                                        {
                                            return f.second.get_entry_point() == entry.entry_point;
                                        });
            if(func_it != mod_it->second.end())
            {
                func_name = func_it->first;
            }
        }

        buf += fmt::format("  in {}.{}\n", entry.mod_name, func_name);
    }
    return buf;
}

/** Function argument writing and destruction. */
class arguments_scope
{
    /** The arguments to manage. */
    const std::vector<value>& args;

    /** The locals. */
    std::vector<std::byte>& locals;

public:
    /**
     * Construct an argument scope. Validates the argument types and creates
     * them in `locals`.
     *
     * @param ctx The associated interpreter context.
     * @param module_name The module's name. Only used for error reporting.
     * @param function_name The function's name. Only used for error reporting.
     * @param args The arguments to verify and write.
     * @param arg_types The argument types to validate against.
     * @param locals The locals storage to write into.
     */
    arguments_scope(context& ctx,
                    const std::string& module_name,
                    const std::string& function_name,
                    const std::vector<value>& args,
                    const std::vector<std::pair<module_::variable_type, bool>>& arg_types,
                    std::vector<std::byte>& locals)
    : args{args}
    , locals{locals}
    {
        if(arg_types.size() != args.size())
        {
            throw interpreter_error(
              fmt::format("Argument count for function '{}.{}' does not match: Expected {}, got {}.",
                          module_name, function_name,
                          arg_types.size(), args.size()));
        }

        std::size_t offset = 0;
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            if(std::get<0>(arg_types[i]) != std::get<0>(args[i].get_type()))
            {
                throw interpreter_error(
                  fmt::format("Argument {} for function '{}.{}' has wrong base type (expected '{}', got '{}').",
                              i,
                              module_name, function_name,
                              std::get<0>(arg_types[i]).base_type(), std::get<0>(args[i].get_type())));
            }

            if(std::get<1>(arg_types[i]) != std::get<1>(args[i].get_type()))
            {
                throw interpreter_error(
                  fmt::format("Argument {} for function '{}.{}' has wrong array property (expected '{}', got '{}').",
                              i,
                              module_name, function_name,
                              std::get<1>(arg_types[i]), std::get<1>(args[i].get_type())));
            }

            if(offset + args[i].get_size() > locals.size())
            {
                throw interpreter_error(fmt::format(
                  "Stack overflow during argument allocation while processing argument {} of function '{}.{}'.",
                  i, module_name, function_name));
            }

            if(is_garbage_collected(args[i].get_type()))
            {
                ctx.get_gc().add_temporary(&locals[offset]);
            }
            offset += args[i].create(&locals[offset]);
        }
    }

    /** Destructor. Destroys the created arguments. */
    ~arguments_scope()
    {
        std::size_t offset = 0;
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            if(offset + args[i].get_size() > locals.size())
            {
                DEBUG_LOG("Stack overflow during argument destruction while processing argument {}.", i);

                // FIXME This is an error, but destructors cannot throw.
                return;
            }

            offset += args[i].destroy(&locals[offset]);
        }
    }
};

value context::exec(
  const std::string& module_name,
  const std::string& function_name,
  const module_::language_module& mod,
  const function& f,
  std::vector<value> args)
{
    /*
     * allocate locals and decode arguments.
     */
    stack_frame frame{mod.get_header().strings, f.get_locals_size(), f.get_stack_size()};

    auto& arg_types = f.get_signature().arg_types;
    arguments_scope arg_scope{*this, module_name, function_name, args, arg_types, frame.locals};

    /*
     * Execute the function.
     */
    opcode ret_opcode;
    if(f.is_native())
    {
        auto& func = f.get_function();
        func(frame.stack);
        ret_opcode = f.get_return_opcode();
    }
    else
    {
        ret_opcode = exec(mod, f.get_entry_point(), f.get_size(), f.get_locals(), frame);
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
        ret = value{frame.stack.pop_i32()};
    }
    else if(ret_opcode == opcode::fret)
    {
        ret = value{frame.stack.pop_f32()};
    }
    else if(ret_opcode == opcode::sret)
    {
        std::string* s = frame.stack.pop_addr<std::string>();
        ret = value{*s};    // copy string
        gc.remove_temporary(s);
    }
    else if(ret_opcode == opcode::aret)
    {
        void* addr = frame.stack.pop_addr<void>();
        ret = value{addr};
        // FIXME The caller is responsible for calling `gc.remove_temporary(addr)`.
    }
    else
    {
        throw interpreter_error(fmt::format(
          "'{}.{}': Invalid return opcode '{}' ({}).",
          module_name, function_name,
          to_string(ret_opcode), static_cast<int>(ret_opcode)));
    }

    // invoke the garbage collector to clean up before returning.
    gc.run();

    // verify that the stack is empty.
    if(!frame.stack.empty())
    {
        throw interpreter_error(fmt::format(
          "'{}.{}': Non-empty stack on function exit.",
          module_name, function_name));
    }

    return ret;
}

void context::register_native_function(const std::string& mod_name, std::string fn_name, std::function<void(operand_stack&)> func)
{
    if(!func)
    {
        throw interpreter_error(fmt::format("Cannot register null native function '{}.{}'.", mod_name, fn_name));
    }

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

void context::load_module(const std::string& name, const module_::language_module& mod)
{
    DEBUG_LOG("load_module: {}", name);

    if(module_map.find(name) != module_map.end())
    {
        throw interpreter_error(fmt::format("Module '{}' already loaded.", name));
    }

    // populate type map before decoding the module.
    std::unordered_map<std::string, module_::struct_descriptor> tmap;
    for(auto& it: mod.header.exports)
    {
        if(it.type != module_::symbol_type::type)
        {
            continue;
        }

        if(tmap.find(it.name) != tmap.end())
        {
            throw interpreter_error(fmt::format("Type '{}' already exists in exports.", it.name));
        }

        tmap.insert({it.name, std::get<module_::struct_descriptor>(it.desc)});
    }

    decode_structs(tmap, mod);
    struct_map.insert({name, std::move(tmap)});

    // decode the module.
    module_map.insert({name, decode(struct_map[name], mod)});
    module_::module_header& decoded_header = module_map[name]->header;

    // populate function map.
    std::unordered_map<std::string, function> fmap;
    for(auto& it: decoded_header.exports)
    {
        if(it.type != module_::symbol_type::function)
        {
            continue;
        }

        if(fmap.find(it.name) != fmap.end())
        {
            throw interpreter_error(fmt::format("Function '{}' already exists in exports.", it.name));
        }

        auto& desc = std::get<module_::function_descriptor>(it.desc);
        if(desc.native)
        {
            auto& details = std::get<module_::native_function_details>(desc.details);
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
            auto& details = std::get<module_::function_details>(desc.details);
            fmap.insert({it.name, function{desc.signature, details.offset, details.size, details.locals, details.locals_size, details.stack_size}});
        }
    }
    function_map.insert({name, std::move(fmap)});
}

value context::invoke(const std::string& module_name, const std::string& function_name, std::vector<value> args)
{
    try
    {
        DEBUG_LOG("invoke: {}.{}", module_name, function_name);

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

        return exec(module_name, function_name, *mod_it->second, func_it->second, std::move(args));
    }
    catch(interpreter_error& e)
    {
        // Update the error message with the stack trace.
        std::string buf = e.what();

        auto stack_trace = e.get_stack_trace();
        if(stack_trace.size() > 0)
        {
            buf += fmt::format("\n{}", stack_trace_to_string(stack_trace));
        }

        throw interpreter_error(buf, stack_trace);
    }
}

}    // namespace slang::interpreter