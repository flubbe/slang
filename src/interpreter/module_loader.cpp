/**
 * slang - a simple scripting language.
 *
 * module loader.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "shared/type_utils.h"
#include "interpreter.h"
#include "package.h"

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

bool is_garbage_collected(const module_::variable_type& t) noexcept
{
    if(t.base_type() == "void")
    {
        return false;
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
static bool is_garbage_collected(const slang::module_::field_descriptor& info) noexcept
{
    return info.base_type.is_array() || is_garbage_collected(info.base_type);
}

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
    if(v.type.base_type().empty())
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

    throw interpreter_error(std::format("Unable to determine type size for '{}'.", v.type.base_type()));
}

/** Get the type size (for built-in types) or the size of a type reference (for custom types). */
static std::size_t get_type_or_reference_size(const module_::variable_type& v)
{
    if(v.base_type().empty())
    {
        throw interpreter_error("Unable to determine type size for empty type.");
    }

    if(v.is_array())
    {
        return sizeof(void*);
    }

    auto built_in_it = type_properties_map.find(v.base_type());
    if(built_in_it != type_properties_map.end())
    {
        return built_in_it->second.first;
    }

    // FIXME Assume all other types are references.
    return sizeof(void*);
}

/**
 * Calculate the stack size delta from a function's signature.
 *
 * @param s The signature.
 * @returns The stack size delta.
 */
static std::int32_t get_stack_delta(const module_::function_signature& s)
{
    auto return_type_size = get_type_or_reference_size(s.return_type);
    std::int32_t arg_size = 0;
    for(const auto& it: s.arg_types)
    {
        arg_size += utils::numeric_cast<std::int32_t>(get_type_or_reference_size(it));
        // NOTE stack contents are not aligned.
    }
    return utils::numeric_cast<std::int32_t>(return_type_size) - arg_size;
}

/**
 * Decode the function's arguments and locals.
 *
 * @param desc The function descriptor.
 */
static void decode_locals(module_::function_descriptor& desc)
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

type_properties module_loader::get_type_properties(const module_::variable_type& type) const
{
    // array types.
    if(type.is_array())
    {
        // FIXME the copy `std::size_t(...)` is here because clang complains about losing `const` qualifier.
        return {0, sizeof(void*), std::size_t{std::alignment_of_v<void*>}, 0};
    }

    // built-in types.
    if(type.base_type() == "void")
    {
        return {0, 0, 0, 0};
    }

    auto built_in_it = type_properties_map.find(type.base_type());
    if(built_in_it != type_properties_map.end())
    {
        return {0, built_in_it->second.first, built_in_it->second.second, 0};
    }

    // structs.
    auto type_it = struct_map.find(type.base_type());
    if(type_it == struct_map.end())
    {
        throw interpreter_error(std::format("Cannot resolve size for type '{}': Type not found.", type.base_type()));
    }

    return {type_it->second.flags, type_it->second.size, type_it->second.alignment, type_it->second.layout_id};
}

field_properties module_loader::get_field_properties(
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
        throw interpreter_error(std::format("Invalid struct type name '{}'.", type_name));
    }

    // structs.
    auto type_it = struct_map.find(type_name);
    if(type_it == struct_map.end())
    {
        throw interpreter_error(std::format("Cannot resolve size for type '{}': Type not found.", type_name));
    }

    auto field_info = type_it->second.member_types.at(field_index);
    return {field_info.second.size, field_info.second.offset, is_garbage_collected(type_name)};
}

void module_loader::decode_structs()
{
    recorder->section("Types");

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
                if(!struct_map.contains(member_type.base_type.base_type()))
                {
                    if(!member_type.base_type.import_index.has_value())
                    {
                        throw interpreter_error(
                          std::format(
                            "Cannot resolve size for type '{}': Type not found.",
                            member_type.base_type.base_type()));
                    }

                    std::size_t index = member_type.base_type.import_index.value();
                    if(index >= mod.header.imports.size())
                    {
                        throw interpreter_error(
                          std::format(
                            "Cannot resolve size for type '{}': Invalid import index {}.",
                            member_type.base_type.base_type(),
                            index));
                    }

                    if(std::get<const module_loader*>(mod.header.imports[index].export_reference) == nullptr)
                    {
                        // load the package containing the type definition.
                        if(mod.header.imports[index].type != module_::symbol_type::type)
                        {
                            throw interpreter_error(
                              std::format(
                                "Cannot resolve size for type '{}': Import table entry {} is not a type.",
                                member_type.base_type.base_type(),
                                index));
                        }

                        std::size_t package_index = mod.header.imports[index].package_index;
                        if(mod.header.imports[package_index].type != module_::symbol_type::package)
                        {
                            throw interpreter_error(
                              std::format(
                                "Cannot resolve size for type '{}': Import table entry {} is not a package.",
                                member_type.base_type.base_type(),
                                package_index));
                        }

                        module_loader& loader = ctx.resolve_module(mod.header.imports[package_index].name);
                        mod.header.imports[package_index].export_reference = &loader;
                    }

                    // verify the type definition is loaded.
                    std::size_t package_index = mod.header.imports[index].package_index;
                    const module_loader* loader = std::get<const module_loader*>(mod.header.imports[package_index].export_reference);
                    if(loader == nullptr)
                    {
                        throw interpreter_error(
                          std::format(
                            "Could not resolve package for import '{}'.",
                            member_type.base_type.base_type()));
                    }
                    if(!loader->struct_map.contains(member_type.base_type.base_type()))
                    {
                        throw interpreter_error(
                          std::format(
                            "Cannot resolve size for type '{}': Type not found in import '{}'.",
                            member_type.base_type.base_type(),
                            mod.header.imports[package_index].name));
                    }
                }

                // size and alignment are the same for both array and non-array types.
                member_type.size = sizeof(void*);
                member_type.alignment = std::alignment_of_v<void*>;

                add_to_layout = true;
            }

            // store offset.
            member_type.offset = utils::align(member_type.alignment, offset);

            // calculate member offset as `size_after - size_before`.
            offset -= utils::numeric_cast<int>(size);

            // update struct size and alignment.
            size += member_type.size;
            size = utils::align(member_type.alignment, size);

            alignment = std::max(alignment, member_type.alignment);

            // calculate member offset as `size_after - size_before`.
            offset += utils::numeric_cast<int>(size);

            // update type layout.
            if(add_to_layout)
            {
                layout.push_back(member_type.offset);
            }
        }

        // trailing padding.
        size = utils::align(alignment, size);

        // store type size and alignment.
        desc.size = size;
        desc.alignment = alignment;

        // check/store layout.
        if((desc.flags & static_cast<std::uint8_t>(module_::struct_flags::native)) != 0)
        {
            desc.layout_id = ctx.get_gc().check_type_layout(make_type_name(import_name, name), layout);
        }
        else
        {
            desc.layout_id = ctx.get_gc().register_type_layout(make_type_name(import_name, name), std::move(layout));
        }

        recorder->type(name, desc);
    }
}

void module_loader::decode()
{
    if(mod.is_decoded())
    {
        throw interpreter_error("Tried to decode a module that already is decoded.");
    }

    memory_read_archive ar{mod.get_binary(), true, slang::endian::little};

    recorder->section("Constant table");
    for(auto& c: mod.header.constants)
    {
        recorder->constant(c);
    }

    /*
     * resolve imports.
     */
    recorder->section("Import table");

    for(auto& it: mod.header.imports)
    {
        recorder->record(it);

        if(it.type == module_::symbol_type::package)
        {
            // packages are loaded while resolving other symbols.
            continue;
        }

        // resolve the symbol's package.
        if(it.package_index >= mod.header.imports.size())
        {
            throw interpreter_error(
              std::format(
                "Error while resolving imports for '{}': Import symbol '{}' has invalid package index ({} >= {}).",
                import_name,
                it.name,
                it.package_index,
                mod.header.imports.size()));
        }

        if(mod.header.imports[it.package_index].type != module_::symbol_type::package)
        {
            throw interpreter_error(
              std::format(
                "Error while resolving imports for '{}': Import symbol '{}' refers to non-package import entry.",
                import_name,
                it.name));
        }

        module_loader& loader = ctx.resolve_module(mod.header.imports[it.package_index].name);
        mod.header.imports[it.package_index].export_reference = &loader;    // FIXME written for all symbols

        // find the imported symbol.
        module_::module_header& import_header = loader.mod.header;
        auto exp_it = std::ranges::find_if(
          import_header.exports,
          [&it](const module_::exported_symbol& exp) -> bool
          {
              return exp.name == it.name;
          });
        if(exp_it == import_header.exports.end())
        {
            throw interpreter_error(
              std::format(
                "Error while resolving imports for '{}': Symbol '{}' is not exported by module '{}'.",
                import_name,
                it.name, loader.get_path().string()));
        }
        if(exp_it->type != it.type)
        {
            throw interpreter_error(
              std::format(
                "Error while resolving imports for '{}': Symbol '{}' from module '{}' has wrong type (expected '{}', got '{}').",
                import_name,
                it.name,
                loader.get_path().string(),
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
            details.func = ctx.resolve_native_function(exp_it->name, details.library_name);
        }
    }

    /*
     * arguments and locals.
     */
    for(auto& it: mod.header.exports)
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
            details.func = ctx.resolve_native_function(it.name, details.library_name);
            continue;
        }

        decode_locals(desc);
    }

    /*
     * instructions.
     */
    recorder->section("Disassembly");

    std::vector<std::byte> code;
    for(auto& it: mod.header.exports)
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

        recorder->function(it.name, details);

        std::int32_t stack_size = 0;
        std::size_t max_stack_size = 0;

        while(ar.tell() < bytecode_end)
        {
            std::byte instr{0};
            ar & instr;

            // don't store non-executable instructions.
            if(static_cast<opcode>(instr) != opcode::label)
            {
                code.push_back(instr);
            }

            stack_size += decode_instruction(ar, instr, details, code);
            if(stack_size < 0)
            {
                throw interpreter_error("Error during decode: Got negative stack size.");
            }

            if(utils::numeric_cast<std::size_t>(stack_size) > max_stack_size)
            {
                max_stack_size = stack_size;
            }
        }

        details.size = code.size() - details.offset;
        details.stack_size = max_stack_size;

        // jump target resolution.
        for(auto& [origin, id]: mod.jump_origins)
        {
            auto target_it = mod.jump_targets.find(id);
            if(target_it == mod.jump_targets.end())
            {
                throw interpreter_error(
                  std::format(
                    "Unable to resolve jump target for label '{}'.",
                    id));
            }
            std::memcpy(&code[origin], &target_it->second, sizeof(std::size_t));
        }
    }

    mod.binary = std::move(code);
    mod.decoded = true;
}

std::int32_t module_loader::decode_instruction(
  archive& ar,
  std::byte instr,
  const module_::function_details& details,
  std::vector<std::byte>& code)
{
    switch(static_cast<opcode>(instr))
    {
    /* opcodes without arguments. */
    case opcode::aconst_null: [[fallthrough]];
    case opcode::adup:
        recorder->record(static_cast<opcode>(instr));
        return static_cast<std::int32_t>(sizeof(void*));
    case opcode::idup: [[fallthrough]];
    case opcode::fdup:
        recorder->record(static_cast<opcode>(instr));
        return static_cast<std::int32_t>(sizeof(std::int32_t));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::pop:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::apop:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*));
    case opcode::arraylength:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*)) + static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::iaload: [[fallthrough]];
    case opcode::faload:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*));
    case opcode::aaload:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)) + static_cast<std::int32_t>(sizeof(void*));
    case opcode::iastore: [[fallthrough]];
    case opcode::fastore:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*)) - (2 * static_cast<std::int32_t>(sizeof(std::int32_t)));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::aastore:
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t)) - static_cast<std::int32_t>(sizeof(void*));
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
        recorder->record(static_cast<opcode>(instr));
        return -static_cast<std::int32_t>(sizeof(std::int32_t));    // same size for all (since sizeof(float) == sizeof(std::int32_t))
    case opcode::acmpeq: [[fallthrough]];
    case opcode::acmpne:
        recorder->record(static_cast<opcode>(instr));
        return -(2 * static_cast<std::int32_t>(sizeof(void*))) + static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::i2f: [[fallthrough]];
    case opcode::f2i: [[fallthrough]];
    case opcode::ret: [[fallthrough]];
    case opcode::iret: [[fallthrough]];
    case opcode::fret: [[fallthrough]];
    case opcode::sret: [[fallthrough]];
    case opcode::aret:
        recorder->record(static_cast<opcode>(instr));
        return 0;
    /* opcodes with one 1-byte argument. */
    case opcode::newarray:
    {
        std::uint8_t i_u8{0};
        ar & i_u8;

        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&i_u8),                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&i_u8) + sizeof(i_u8));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), static_cast<std::int64_t>(i_u8));
        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32{0};
        ar & i_u32;

        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&i_u32),                     // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&i_u32) + sizeof(i_u32));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        if(static_cast<opcode>(instr) == opcode::iconst)
        {
            recorder->record(opcode::iconst, static_cast<std::int64_t>(i_u32));
        }
        else
        {
            float f{0.0};
            std::memcpy(&f, &i_u32, sizeof(float));
            recorder->record(opcode::fconst, f);
        }

        return static_cast<std::int32_t>(sizeof(std::uint32_t));
    }
    /* opcodes with one VLE integer. */
    case opcode::sconst:
    {
        vle_int i;
        ar & i;

        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&i.i),                   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&i.i) + sizeof(i.i));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), i.i);
        return static_cast<std::int32_t>(sizeof(std::string*));
    }
    case opcode::label:
    {
        vle_int i;
        ar & i;

        // store jump target for later resolution.
        mod.jump_targets.insert({i.i, code.size()});

        recorder->label(i.i);
        return 0;
    }
    case opcode::jmp:
    {
        vle_int i;
        ar & i;

        // store jump origin for later resolution and write zeros instead.
        std::size_t z = 0;
        mod.jump_origins.insert({code.size(), i.i});
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&z),                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&z) + sizeof(z));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), i.i);
        return 0;
    }
    /* opcodes with two VLE integers. */
    case opcode::jnz:
    {
        vle_int i1;
        vle_int i2;
        ar & i1 & i2;

        // store jump origin for later resolution and write zeros instead.
        std::size_t z = 0;
        mod.jump_origins.insert({code.size(), i1.i});
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&z),                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&z) + sizeof(z));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        mod.jump_origins.insert({code.size(), i2.i});
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&z),                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&z) + sizeof(z));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), i1.i, i2.i);
        return -static_cast<std::int32_t>(sizeof(std::int32_t));
    }
    /* dup_x1. */
    case opcode::dup_x1:
    {
        // type arguments.
        module_::variable_type t1;
        module_::variable_type t2;
        ar & t1 & t2;

        // "void" is not allowed.
        if(t1.base_type() == "void" || t2.base_type() == "void")
        {
            throw interpreter_error("Error decoding dup_x1 instruction: Invalid argument type 'void'.");
        }

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto properties1 = get_type_properties(t1);
        auto properties2 = get_type_properties(t2);

        // check if the type needs garbage collection.
        std::uint8_t needs_gc = is_garbage_collected(t1) ? 1 : 0;

        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&properties1.size),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&properties1.size) + sizeof(properties1.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&properties2.size),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&properties2.size) + sizeof(properties2.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&needs_gc),                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&needs_gc) + sizeof(needs_gc));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), to_string(t1), to_string(t2));
        return static_cast<std::int32_t>(properties1.size);
    }
    /* dup_x2. */
    case opcode::dup_x2:
    {
        // type arguments.
        module_::variable_type t1;
        module_::variable_type t2;
        module_::variable_type t3;
        ar & t1 & t2 & t3;

        // "void" is not allowed.
        if(t1.base_type() == "void" || t2.base_type() == "void" || t3.base_type() == "void")
        {
            throw interpreter_error("Error decoding dup_x2 instruction: Invalid argument type 'void'.");
        }

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto properties1 = get_type_properties(t1);
        auto properties2 = get_type_properties(t2);
        auto properties3 = get_type_properties(t3);

        // check if the type needs garbage collection.
        std::uint8_t needs_gc = is_garbage_collected(t1) ? 1 : 0;

        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&properties1.size),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&properties1.size) + sizeof(properties1.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&properties2.size),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&properties2.size) + sizeof(properties2.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&properties3.size),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&properties3.size) + sizeof(properties3.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&needs_gc),                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&needs_gc) + sizeof(needs_gc));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

        recorder->record(static_cast<opcode>(instr), to_string(t1), to_string(t2), to_string(t3));
        return static_cast<std::int32_t>(properties1.size);
    } /* invoke. */
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
                throw interpreter_error(std::format("Import index {} out of range ({} >= {}).", import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::function)
            {
                throw interpreter_error(std::format("Cannot resolve call: Import header at index {} does not refer to a function.", -i.i - 1));
            }

            auto& exp_symbol = std::get<module_::exported_symbol*>(imp_symbol.export_reference);
            if(exp_symbol->type != imp_symbol.type)
            {
                throw interpreter_error(
                  std::format(
                    "Cannot resolve call: Export type for '{}' does not match import type ({} != {}).",
                    imp_symbol.name,
                    slang::module_::to_string(exp_symbol->type),
                    slang::module_::to_string(imp_symbol.type)));
            }

            auto& desc = std::get<module_::function_descriptor>(exp_symbol->desc);

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(
                  std::format("Package import index {} out of range ({} >= {}).",
                              imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            const module_loader* loader_ptr =
              std::get<const module_loader*>(mod.header.imports[imp_symbol.package_index].export_reference);
            code.insert(
              code.end(),
              reinterpret_cast<const std::byte*>(&loader_ptr),                          // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<const std::byte*>(&loader_ptr) + sizeof(loader_ptr));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-sizeof-expression)

            const module_::function_descriptor* desc_ptr = &desc;
            code.insert(
              code.end(),
              reinterpret_cast<const std::byte*>(&desc_ptr),                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-sizeof-expression)

            if(loader_ptr == nullptr)
            {
                throw interpreter_error(std::format("Unresolved module import '{}'.", mod.header.imports[imp_symbol.package_index].name));
            }

            recorder->record(static_cast<opcode>(instr), i.i, imp_symbol.name);
            return get_stack_delta(desc.signature);
        }

        /*
         * i.i >= 0.
         */

        if(static_cast<std::size_t>(i.i) >= mod.header.exports.size())
        {
            throw interpreter_error(std::format("Export index {} out of range ({} >= {}).", i.i, i.i, mod.header.exports.size()));
        }

        auto& exp_symbol = mod.header.exports[i.i];
        if(exp_symbol.type != module_::symbol_type::function)
        {
            throw interpreter_error(std::format("Cannot resolve call: Header entry at index {} is not a function.", i.i));
        }
        auto& desc = std::get<module_::function_descriptor>(exp_symbol.desc);

        const module_loader* loader_ptr = this;
        code.insert(
          code.end(),
          reinterpret_cast<const std::byte*>(&loader_ptr),                          // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<const std::byte*>(&loader_ptr) + sizeof(loader_ptr));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-sizeof-expression)

        const module_::function_descriptor* desc_ptr = &desc;
        code.insert(
          code.end(),
          reinterpret_cast<const std::byte*>(&desc_ptr),                        // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<const std::byte*>(&desc_ptr) + sizeof(desc_ptr));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-sizeof-expression)

        if(desc.native && !static_cast<bool>(std::get<1>(desc.details).func))
        {
            throw interpreter_error("Native function was null during decode.");
        }

        recorder->record(static_cast<opcode>(instr), i.i, exp_symbol.name);
        return get_stack_delta(desc.signature);
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
            throw interpreter_error(std::format("Index '{}' for argument or local outside of valid range 0-{}.", i.i, details.locals.size()));
        }

        auto offset = utils::numeric_cast<std::int64_t>(details.locals[i.i].offset);
        code.insert(
          code.end(),
          reinterpret_cast<std::byte*>(&offset),                      // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
          reinterpret_cast<std::byte*>(&offset) + sizeof(offset));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic,bugprone-sizeof-expression)

        recorder->record(static_cast<opcode>(instr), i.i);

        // return correct size.
        bool is_store = (static_cast<opcode>(instr) == opcode::istore)
                        || (static_cast<opcode>(instr) == opcode::fstore)
                        || (static_cast<opcode>(instr) == opcode::astore);
        bool is_ref = (static_cast<opcode>(instr) == opcode::aload)
                      || (static_cast<opcode>(instr) == opcode::astore);

        if(is_ref)
        {
            return is_store
                     ? -static_cast<std::int32_t>(sizeof(void*))
                     : static_cast<std::int32_t>(sizeof(void*));
        }

        return is_store
                 ? -static_cast<std::int32_t>(sizeof(std::uint32_t))
                 : static_cast<std::int32_t>(sizeof(std::uint32_t));    // same size for i32/f32 (since sizeof(float) == sizeof(std::uint32_t))
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
            if(import_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            module_loader& loader = ctx.resolve_module(imp_package.name);
            auto properties = loader.get_type_properties(imp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.size),                               // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.alignment),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.alignment) + sizeof(properties.alignment));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), i.i, imp_symbol.name);
        }
        else
        {
            // exported type.

            if(static_cast<std::size_t>(i.i) >= mod.header.exports.size())
            {
                throw interpreter_error(std::format("Export index {} out of range ({} >= {}).",
                                                    i.i, i.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[i.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Export header entry at index {} is not a type.",
                  i.i));
            }

            auto properties = get_type_properties(exp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.size),                               // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.alignment),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.alignment) + sizeof(properties.alignment));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), i.i, exp_symbol.name);
        }

        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* anewarray. */
    case opcode::anewarray:
    {
        vle_int i;
        ar & i;

        if(i.i < 0)
        {
            // imported type.
            std::size_t import_index = -i.i - 1;
            if(import_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            module_loader& loader = ctx.resolve_module(imp_package.name);
            auto properties = loader.get_type_properties(imp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), i.i, imp_symbol.name);
        }
        else
        {
            // exported type.

            if(static_cast<std::size_t>(i.i) >= mod.header.exports.size())
            {
                throw interpreter_error(std::format("Export index {} out of range ({} >= {}).",
                                                    i.i, i.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[i.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Export header entry at index {} is not a type.",
                  i.i));
            }

            auto properties = get_type_properties(exp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), i.i, exp_symbol.name);
        }

        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* setfield, getfield. */
    case opcode::setfield: [[fallthrough]];
    case opcode::getfield:
    {
        vle_int struct_index;
        vle_int field_index;
        ar & struct_index & field_index;

        field_properties properties;

        if(struct_index.i < 0)
        {
            // imported type.
            std::size_t import_index = -struct_index.i - 1;
            if(import_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            module_loader& loader = ctx.resolve_module(imp_package.name);
            properties = loader.get_field_properties(imp_symbol.name, field_index.i);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.size),                               // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.offset),                                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.offset) + sizeof(properties.offset));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.needs_gc),                                   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.needs_gc) + sizeof(properties.needs_gc));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), struct_index.i, imp_symbol.name, field_index.i);
        }
        else
        {
            if(static_cast<std::size_t>(struct_index.i) >= mod.header.exports.size())
            {
                throw interpreter_error(std::format("Export index {} out of range ({} >= {}).", struct_index.i, struct_index.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[struct_index.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format("Cannot resolve type: Header entry at index {} is not a type.", struct_index.i));
            }

            properties = get_field_properties(exp_symbol.name, field_index.i);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.size),                               // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.size) + sizeof(properties.size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.offset),                                 // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.offset) + sizeof(properties.offset));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.needs_gc),                                   // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.needs_gc) + sizeof(properties.needs_gc));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), struct_index.i, exp_symbol.name, field_index.i);
        }

        if(static_cast<opcode>(instr) == opcode::setfield)
        {
            return -static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(properties.size);
        }

        return -static_cast<std::int32_t>(sizeof(void*)) + static_cast<std::int32_t>(properties.size);
    }
    /* checkcast */
    case opcode::checkcast:
    {
        vle_int struct_index;
        ar & struct_index;

        if(struct_index.i < 0)
        {
            // imported type.
            std::size_t import_index = -struct_index.i - 1;
            if(import_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Import index {} out of range ({} >= {}).",
                  import_index, import_index, mod.header.imports.size()));
            }

            auto& imp_symbol = mod.header.imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Import header entry at index {} is not a type.",
                  import_index));
            }

            if(imp_symbol.package_index >= mod.header.imports.size())
            {
                throw interpreter_error(std::format(
                  "Package import index {} out of range ({} >= {}).",
                  imp_symbol.package_index, imp_symbol.package_index, mod.header.imports.size()));
            }

            auto& imp_package = mod.header.imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve package: Import header entry at index {} is not a package.",
                  imp_symbol.package_index));
            }

            module_loader& loader = ctx.resolve_module(imp_package.name);
            auto properties = loader.get_type_properties(imp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.flags),                                // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.flags) + sizeof(properties.flags));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), struct_index.i, imp_symbol.name);
        }
        else
        {
            // exported type.

            if(static_cast<std::size_t>(struct_index.i) >= mod.header.exports.size())
            {
                throw interpreter_error(std::format("Export index {} out of range ({} >= {}).",
                                                    struct_index.i, struct_index.i, mod.header.exports.size()));
            }

            auto& exp_symbol = mod.header.exports[struct_index.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(std::format(
                  "Cannot resolve type: Export header entry at index {} is not a type.",
                  struct_index.i));
            }

            auto properties = get_type_properties(exp_symbol.name);
            code.insert(
              code.end(),
              reinterpret_cast<std::byte*>(&properties.layout_id),                                    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<std::byte*>(&properties.layout_id) + sizeof(properties.layout_id));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)

            recorder->record(static_cast<opcode>(instr), struct_index.i, exp_symbol.name);
        }

        return 0; /* no stack size change */
    }
    default:
        throw interpreter_error(std::format("Unexpected opcode '{}' ({}) during decode.", to_string(static_cast<opcode>(instr)), static_cast<int>(instr)));
    }
}

void module_loader::resolve_type(module_::variable_type& type) const
{
    if(ty::is_builtin_type(type.base_type()))
    {
        if(type.import_index.has_value())
        {
            throw interpreter_error(
              std::format(
                "Built-in type '{}' cannot have an import index.",
                type.base_type()));
        }
    }
    else if(type.import_index.has_value())
    {
        const module_::imported_symbol& imp_type = mod.header.imports.at(type.import_index.value());
        const module_::imported_symbol& imp_pkg = mod.header.imports.at(imp_type.package_index);

        std::string type_name = make_type_name(imp_pkg.name, imp_type.name);
        if(imp_pkg.type != module_::symbol_type::package)
        {
            throw interpreter_error(
              std::format(
                "Could not resolve import '{}': Invalid symbol type for package.",
                type_name));
        }

        type.layout_id = ctx.get_gc().get_type_layout_id(type_name);
    }
    else
    {
        // check that the type is contained in the current module.
        type.layout_id = ctx.get_gc().get_type_layout_id(make_type_name(import_name, type.base_type()));
    }
};

module_loader::module_loader(
  context& ctx,
  std::string import_name,
  fs::path path,
  std::shared_ptr<instruction_recorder> recorder)
: ctx{ctx}
, import_name{std::move(import_name)}
, path{std::move(path)}
, recorder{std::move(recorder)}
{
    // make sure we have an instruction recorder.
    if(!this->recorder)
    {
        throw interpreter_error("Instruction recorder cannot be set to null.");
    }

    auto read_ar = ctx.file_mgr.open(this->path, slang::file_manager::open_mode::read);
    (*read_ar) & mod;

    // populate type map before decoding the module.
    this->recorder->section("Export table");
    for(auto& it: mod.header.exports)
    {
        this->recorder->record(it);

        if(it.type != module_::symbol_type::type)
        {
            continue;
        }

        struct_map.insert({it.name, std::get<module_::struct_descriptor>(it.desc)});
    }
    decode_structs();

    // decode the module.
    decode();

    // populate function map.
    for(auto& it: mod.header.exports)
    {
        if(it.type != module_::symbol_type::function)
        {
            continue;
        }

        if(function_map.contains(it.name))
        {
            throw interpreter_error(std::format("Function '{}' already exists in exports.", it.name));
        }

        auto& desc = std::get<module_::function_descriptor>(it.desc);

        /*
         * resolve layout id's in the function signature. these are used for validation
         * when functions are called from native code.
         */

        resolve_type(desc.signature.return_type);
        for(auto& arg: desc.signature.arg_types)
        {
            resolve_type(arg);
        }

        /*
         * resolve native functions.
         */

        if(desc.native)
        {
            auto& details = std::get<module_::native_function_details>(desc.details);
            auto mod_it = ctx.native_function_map.find(details.library_name);
            if(mod_it == ctx.native_function_map.end())
            {
                // TODO Add e.g. a callback to resolve the library?
                throw interpreter_error(std::format("Cannot resolve module '{}' containing native function '{}'.", details.library_name, it.name));
            }

            auto func_it = mod_it->second.find(it.name);
            if(func_it == mod_it->second.end())
            {
                throw interpreter_error(std::format("Cannot resolve native function '{}' in module '{}'.", it.name, details.library_name));
            }

            function_map.insert(
              {it.name,
               function{
                 ctx,
                 *this,
                 desc.signature,
                 func_it->second}});
        }
        else
        {
            auto& details = std::get<module_::function_details>(desc.details);
            function_map.insert(
              {it.name,
               function{
                 ctx,
                 *this,
                 desc.signature,
                 details.offset,
                 details.size,
                 details.locals,
                 details.locals_size,
                 details.stack_size}});
        }
    }
}

bool module_loader::has_function(const std::string& name) const
{
    return function_map.find(name) != function_map.cend();
}

function& module_loader::get_function(const std::string& name)
{
    auto it = function_map.find(name);
    if(it == function_map.end())
    {
        throw interpreter_error(std::format("Cannot find function '{}' in module '{}'.", name, path.string()));
    }
    return it->second;
}

std::optional<std::string> module_loader::resolve_entry_point(std::size_t entry_point) const
{
    auto func_it = std::ranges::find_if(
      std::as_const(function_map),
      [entry_point](const auto& f) -> bool
      {
          return f.second.get_entry_point() == entry_point;
      });
    if(func_it != function_map.cend())
    {
        return func_it->first;
    }

    return std::nullopt;
}

}    // namespace slang::interpreter
