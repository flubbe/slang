/**
 * slang - a simple scripting language.
 *
 * Control Flow Graph analysis for the interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "archives/archive.h"
#include "shared/opcodes.h"
#include "shared/type_class.h"
#include "cfg.h"
#include "module_loader.h"
#include "stack.h"
#include "types.h"
#include "utils.h"

namespace
{

using namespace slang;
using slang::interpreter::field_properties;
using slang::interpreter::interpreter_error;
using slang::interpreter::module_loader;
using slang::module_::exported_symbol;
using slang::module_::imported_symbol;

/** Byte sizes and alignments for built-in types. */
// FIXME Duplicated from module_loader.cpp
const auto& type_properties_map()
{
    static const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> map{
      {"void", {0, 0}},
      {"i8", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},      // cat1
      {"i16", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},     // cat1
      {"i32", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},     // cat1
      {"i64", {sizeof(std::int64_t), std::alignment_of_v<std::int64_t>}},     // cat2
      {"f32", {sizeof(float), std::alignment_of_v<float>}},                   // cat1
      {"f64", {sizeof(double), std::alignment_of_v<double>}},                 // cat2
      {"str", {sizeof(std::string*), std::alignment_of_v<std::string*>}}};    // ref

    return map;
}

/** Get the type size (for built-in types) or the size of a type reference (for custom types). */
// FIXME Duplicated from module_loader.cpp
std::size_t get_type_or_reference_size(
  const module_::variable_type& v)
{
    if(v.base_type().empty())
    {
        throw interpreter_error("Unable to determine type size for empty type.");
    }

    if(v.is_array())
    {
        return sizeof(void*);
    }

    auto built_in_it = type_properties_map().find(v.base_type());
    if(built_in_it != type_properties_map().end())
    {
        return built_in_it->second.first;
    }

    // FIXME Assume all other types are references.
    return sizeof(void*);
}

/** JNZ condition size on stack. */
constexpr auto jnz_condition_size = static_cast<std::int32_t>(sizeof(std::int32_t));

/**
 * Calculate the stack size delta from a function's signature.
 *
 * @param s The signature.
 * @returns The stack size delta.
 */
// FIXME Duplicated from module_loader.cpp
std::int32_t get_stack_delta(
  const module_::function_signature& s)
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

std::int32_t get_opcode_stack_delta(
  const module_loader& loader,
  opcode op,
  archive& ar,
  const std::__1::vector<exported_symbol>& exports,
  const std::__1::vector<imported_symbol>& imports,
  const std::function<const module_loader&(const std::string&)>& resolver)
{
    switch(op)
    {
    /* opcodes without arguments. */
    case opcode::aconst_null: [[fallthrough]];
    case opcode::adup:
        return static_cast<std::int32_t>(sizeof(void*));
    case opcode::dup:
        return static_cast<std::int32_t>(sizeof(std::int32_t));
    case opcode::dup2:
        return static_cast<std::int32_t>(sizeof(std::int64_t));
    case opcode::pop:
        return static_cast<std::int32_t>(-sizeof(std::int32_t));
    case opcode::pop2:
        return static_cast<std::int32_t>(-sizeof(std::int64_t));
    case opcode::apop:
        return static_cast<std::int32_t>(-sizeof(void*));
    case opcode::arraylength:
        return static_cast<std::int32_t>(-sizeof(void*) + sizeof(std::int32_t));
    case opcode::caload: [[fallthrough]];
    case opcode::saload: [[fallthrough]];
    case opcode::iaload: [[fallthrough]];
    case opcode::faload:
        return static_cast<std::int32_t>(-sizeof(void*));
    case opcode::laload: [[fallthrough]];
    case opcode::daload:
        return static_cast<std::int32_t>(-sizeof(void*) - sizeof(std::int32_t) + sizeof(std::int64_t));
    case opcode::aaload:
        return static_cast<std::int32_t>(-sizeof(void*) - sizeof(std::int32_t) + sizeof(void*));
    case opcode::castore: [[fallthrough]];
    case opcode::sastore: [[fallthrough]];
    case opcode::iastore: [[fallthrough]];
    case opcode::fastore:
        return static_cast<std::int32_t>(-sizeof(void*) - 2 * sizeof(std::int32_t));
    case opcode::lastore: [[fallthrough]];
    case opcode::dastore:
        return static_cast<std::int32_t>(-sizeof(void*) - sizeof(std::int32_t) - sizeof(std::int64_t));
    case opcode::aastore:
        return static_cast<std::int32_t>(-sizeof(void*) - sizeof(std::int32_t) - sizeof(void*));
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
    case opcode::lshl: [[fallthrough]];
    case opcode::ishr: [[fallthrough]];
    case opcode::lshr: [[fallthrough]];
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
        return static_cast<std::int32_t>(-sizeof(std::int32_t));
    case opcode::ladd: [[fallthrough]];
    case opcode::dadd: [[fallthrough]];
    case opcode::lsub: [[fallthrough]];
    case opcode::dsub: [[fallthrough]];
    case opcode::lmul: [[fallthrough]];
    case opcode::dmul: [[fallthrough]];
    case opcode::ldiv: [[fallthrough]];
    case opcode::ddiv: [[fallthrough]];
    case opcode::lmod: [[fallthrough]];
    case opcode::land: [[fallthrough]];
    case opcode::lor: [[fallthrough]];
    case opcode::lxor:
        return static_cast<std::int32_t>(-sizeof(std::int64_t));
    case opcode::lcmpl: [[fallthrough]];
    case opcode::dcmpl: [[fallthrough]];
    case opcode::lcmple: [[fallthrough]];
    case opcode::dcmple: [[fallthrough]];
    case opcode::lcmpg: [[fallthrough]];
    case opcode::dcmpg: [[fallthrough]];
    case opcode::lcmpge: [[fallthrough]];
    case opcode::dcmpge: [[fallthrough]];
    case opcode::lcmpeq: [[fallthrough]];
    case opcode::dcmpeq: [[fallthrough]];
    case opcode::lcmpne: [[fallthrough]];
    case opcode::dcmpne:
        return static_cast<std::int32_t>(-2 * sizeof(std::int64_t) + sizeof(std::int32_t));
    case opcode::acmpeq: [[fallthrough]];
    case opcode::acmpne:
        return static_cast<std::int32_t>(-2 * sizeof(void*) + sizeof(std::int32_t));
    case opcode::ineg: [[fallthrough]];
    case opcode::lneg: [[fallthrough]];
    case opcode::fneg: [[fallthrough]];
    case opcode::dneg: [[fallthrough]];
    case opcode::i2c: [[fallthrough]];
    case opcode::i2s: [[fallthrough]];
    case opcode::i2f: [[fallthrough]];
    case opcode::l2d: [[fallthrough]];
    case opcode::f2i: [[fallthrough]];
    case opcode::d2l: [[fallthrough]];
    case opcode::ret: [[fallthrough]];
    case opcode::iret: [[fallthrough]];
    case opcode::lret: [[fallthrough]];
    case opcode::fret: [[fallthrough]];
    case opcode::dret: [[fallthrough]];
    case opcode::aret:
        return 0;
    case opcode::i2l: [[fallthrough]];
    case opcode::i2d: [[fallthrough]];
    case opcode::f2l: [[fallthrough]];
    case opcode::f2d:
        return static_cast<std::int32_t>(-sizeof(std::int32_t) + sizeof(std::int64_t));
    case opcode::l2i: [[fallthrough]];
    case opcode::l2f: [[fallthrough]];
    case opcode::d2i: [[fallthrough]];
    case opcode::d2f:
        return static_cast<std::int32_t>(-sizeof(std::int64_t) + sizeof(std::int32_t));
    /* opcodes with one 1-byte argument. */
    case opcode::newarray:
    {
        std::uint8_t i_u8{0};
        ar & i_u8;

        return static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t));
    }
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32{0};
        ar & i_u32;

        return static_cast<std::int32_t>(sizeof(std::int32_t));
    }
    /* opcodes with one 8-byte argument. */
    case opcode::lconst: [[fallthrough]];
    case opcode::dconst:
    {
        std::uint64_t i_u64{0};
        ar & i_u64;

        return static_cast<std::int32_t>(sizeof(std::int64_t));
    }
    /* opcodes with one VLE integer. */
    case opcode::sconst:
    {
        vle_int i;
        ar & i;

        return static_cast<std::int32_t>(sizeof(std::string*));
    }
    case opcode::label:
    {
        vle_int i;
        ar & i;

        return 0;
    }
    case opcode::jmp:
    {
        vle_int i;
        ar & i;

        return 0;
    }
    /* opcodes with two VLE integers. */
    case opcode::jnz:
    {
        vle_int i1;
        vle_int i2;
        ar & i1 & i2;

        return -jnz_condition_size;
    }
    /* dup_x1. */
    case opcode::dup_x1:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto layout1 = target_type_layout::for_class(v1);

        return static_cast<std::int32_t>(layout1.size);
    }
    /* dup2_x0. */
    case opcode::dup2_x0:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto layout1 = target_type_layout::for_class(v1);
        auto layout2 = target_type_layout::for_class(v2);

        // convert to std::size_t to align with interpreter.
        std::size_t size1 = layout1.size;
        std::size_t size2 = layout2.size;

        return static_cast<std::int32_t>(size1 + size2);
    }
    /* dup_x2. */
    case opcode::dup_x2:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v3;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2 & v3;

        // decode the types into their sizes. only built-in types (excluding 'void') are allowed.
        auto layout1 = target_type_layout::for_class(v1);
        std::size_t size1 = layout1.size;

        return static_cast<std::int32_t>(size1);
    } /* dup_x2. */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        // resolve to address.
        if(i.i < 0)
        {
            std::int64_t import_index = -i.i - 1;    // this is bounded by 0 from below by the `if` check.
            if(static_cast<std::size_t>(import_index) >= imports.size())
            {
                throw interpreter_error(
                  std::format(
                    "Import index {} out of range ({} >= {}).",
                    import_index,
                    import_index,
                    imports.size()));
            }

            const auto& imp_symbol = imports[import_index];
            if(imp_symbol.type != module_::symbol_type::function)
            {
                throw interpreter_error(
                  std::format(
                    "Cannot resolve call: Import header at index {} does not refer to a function.",
                    -i.i - 1));
            }

            const auto& exp_symbol = std::get<module_::exported_symbol*>(imp_symbol.export_reference);
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

            if(imp_symbol.package_index >= imports.size())
            {
                throw interpreter_error(
                  std::format(
                    "Package import index {} out of range ({} >= {}).",
                    imp_symbol.package_index,
                    imp_symbol.package_index,
                    imports.size()));
            }

            const auto* loader_ptr =
              std::get<const module_loader*>(imports[imp_symbol.package_index].export_reference);

            if(loader_ptr == nullptr)
            {
                throw interpreter_error(
                  std::format(
                    "Unresolved module import '{}'.",
                    imports[imp_symbol.package_index].name));
            }

            return get_stack_delta(desc.signature);
        }

        /*
         * i.i >= 0.
         */

        if(static_cast<std::size_t>(i.i) >= exports.size())
        {
            throw interpreter_error(
              std::format(
                "Export index {} out of range ({} >= {}).",
                i.i,
                i.i,
                exports.size()));
        }

        auto& exp_symbol = exports[i.i];
        if(exp_symbol.type != module_::symbol_type::function)
        {
            throw interpreter_error(
              std::format(
                "Cannot resolve call: Header entry at index {} is not a function.",
                i.i));
        }
        const auto& desc = std::get<module_::function_descriptor>(exp_symbol.desc);

        if(desc.native
           && std::get<1>(desc.details).func == nullptr)
        {
            throw interpreter_error("Native function was null during decode.");
        }

        return get_stack_delta(desc.signature);
    }
    /* opcodes that need to resolve a variable. */
    case opcode::iload: [[fallthrough]];
    case opcode::lload: [[fallthrough]];
    case opcode::fload: [[fallthrough]];
    case opcode::dload: [[fallthrough]];
    case opcode::aload: [[fallthrough]];
    case opcode::istore: [[fallthrough]];
    case opcode::lstore: [[fallthrough]];
    case opcode::fstore: [[fallthrough]];
    case opcode::dstore: [[fallthrough]];
    case opcode::astore:
    {
        vle_int i;
        ar & i;

        // return correct size.
        bool is_store =
          (op == opcode::istore)
          || (op == opcode::lstore)
          || (op == opcode::fstore)
          || (op == opcode::dstore)
          || (op == opcode::astore);

        if((op == opcode::aload)
           || (op == opcode::astore))
        {
            // references.
            return is_store
                     ? static_cast<std::int32_t>(-sizeof(void*))
                     : static_cast<std::int32_t>(sizeof(void*));
        }

        if((op == opcode::lload)
           || (op == opcode::dload)
           || (op == opcode::lstore)
           || (op == opcode::dstore))
        {
            // cat2 type.
            return is_store
                     ? static_cast<std::int32_t>(-sizeof(std::int64_t))
                     : static_cast<std::int32_t>(sizeof(std::int64_t));
        }

        // cat1 type.
        return is_store
                 ? static_cast<std::int32_t>(-sizeof(std::int32_t))
                 : static_cast<std::int32_t>(sizeof(std::int32_t));
    }
    /* new. */
    case opcode::new_:
    {
        vle_int i;
        ar & i;

        return static_cast<std::int32_t>(sizeof(void*));
    }
    /* anewarray. */
    case opcode::anewarray:
    {
        vle_int i;
        ar & i;

        return static_cast<std::int32_t>(sizeof(void*)) - static_cast<std::int32_t>(sizeof(std::int32_t));
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
            if(import_index >= imports.size())
            {
                throw interpreter_error(
                  std::format(
                    "Import index {} out of range ({} >= {}).",
                    import_index,
                    import_index,
                    imports.size()));
            }

            const auto& imp_symbol = imports[import_index];
            if(imp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(
                  std::format(
                    "Cannot resolve type: Import header entry at index {} is not a type.",
                    import_index));
            }

            if(imp_symbol.package_index >= imports.size())
            {
                throw interpreter_error(
                  std::format(
                    "Package import index {} out of range ({} >= {}).",
                    imp_symbol.package_index,
                    imp_symbol.package_index,
                    imports.size()));
            }

            auto& imp_package = imports[imp_symbol.package_index];
            if(imp_package.type != module_::symbol_type::package)
            {
                throw interpreter_error(
                  std::format(
                    "Cannot resolve package: Import header entry at index {} is not a package.",
                    imp_symbol.package_index));
            }

            const module_loader& loader = resolver(imp_package.name);
            properties = loader.get_field_properties(imp_symbol.name, field_index.i);
        }
        else
        {
            if(static_cast<std::size_t>(struct_index.i) >= exports.size())
            {
                throw interpreter_error(
                  std::format(
                    "Export index {} out of range ({} >= {}).",
                    struct_index.i,
                    struct_index.i,
                    exports.size()));
            }

            auto& exp_symbol = exports[struct_index.i];
            if(exp_symbol.type != module_::symbol_type::type)
            {
                throw interpreter_error(
                  std::format(
                    "Cannot resolve type: Header entry at index {} is not a type.",
                    struct_index.i));
            }

            properties = loader.get_field_properties(exp_symbol.name, field_index.i);
        }

        if(op == opcode::setfield)
        {
            return static_cast<std::int32_t>(
              -sizeof(void*) - utils::get_stack_slot_size(properties.size));
        }

        return static_cast<std::int32_t>(-sizeof(void*) + utils::get_stack_slot_size(properties.size));
    }
    /* checkcast */
    case opcode::checkcast:
    {
        vle_int struct_index;
        ar & struct_index;

        return 0; /* no stack size change */
    }
    default:
        throw interpreter_error(
          std::format(
            "Unexpected opcode '{}' ({}) during decode.",
            to_string(op),
            std::to_underlying(op)));
    }
}

/**
 * Skip instruction operands.
 *
 * @param op The instruction.
 * @param ar The archive holding the bytecode.
 */
void skip_operands(
  opcode op,
  archive& ar)
{
    switch(op)
    {
    /* opcodes without arguments. */
    case opcode::aconst_null: [[fallthrough]];
    case opcode::adup: [[fallthrough]];
    case opcode::dup: [[fallthrough]];
    case opcode::dup2: [[fallthrough]];
    case opcode::pop: [[fallthrough]];
    case opcode::pop2: [[fallthrough]];
    case opcode::apop: [[fallthrough]];
    case opcode::arraylength: [[fallthrough]];
    case opcode::caload: [[fallthrough]];
    case opcode::saload: [[fallthrough]];
    case opcode::iaload: [[fallthrough]];
    case opcode::faload: [[fallthrough]];
    case opcode::laload: [[fallthrough]];
    case opcode::daload: [[fallthrough]];
    case opcode::aaload: [[fallthrough]];
    case opcode::castore: [[fallthrough]];
    case opcode::sastore: [[fallthrough]];
    case opcode::iastore: [[fallthrough]];
    case opcode::fastore: [[fallthrough]];
    case opcode::lastore: [[fallthrough]];
    case opcode::dastore: [[fallthrough]];
    case opcode::aastore: [[fallthrough]];
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
    case opcode::lshl: [[fallthrough]];
    case opcode::ishr: [[fallthrough]];
    case opcode::lshr: [[fallthrough]];
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
    case opcode::fcmpne: [[fallthrough]];
    case opcode::ladd: [[fallthrough]];
    case opcode::dadd: [[fallthrough]];
    case opcode::lsub: [[fallthrough]];
    case opcode::dsub: [[fallthrough]];
    case opcode::lmul: [[fallthrough]];
    case opcode::dmul: [[fallthrough]];
    case opcode::ldiv: [[fallthrough]];
    case opcode::ddiv: [[fallthrough]];
    case opcode::lmod: [[fallthrough]];
    case opcode::land: [[fallthrough]];
    case opcode::lor: [[fallthrough]];
    case opcode::lxor: [[fallthrough]];
    case opcode::lcmpl: [[fallthrough]];
    case opcode::dcmpl: [[fallthrough]];
    case opcode::lcmple: [[fallthrough]];
    case opcode::dcmple: [[fallthrough]];
    case opcode::lcmpg: [[fallthrough]];
    case opcode::dcmpg: [[fallthrough]];
    case opcode::lcmpge: [[fallthrough]];
    case opcode::dcmpge: [[fallthrough]];
    case opcode::lcmpeq: [[fallthrough]];
    case opcode::dcmpeq: [[fallthrough]];
    case opcode::lcmpne: [[fallthrough]];
    case opcode::dcmpne: [[fallthrough]];
    case opcode::acmpeq: [[fallthrough]];
    case opcode::acmpne: [[fallthrough]];
    case opcode::ineg: [[fallthrough]];
    case opcode::lneg: [[fallthrough]];
    case opcode::fneg: [[fallthrough]];
    case opcode::dneg: [[fallthrough]];
    case opcode::i2c: [[fallthrough]];
    case opcode::i2s: [[fallthrough]];
    case opcode::i2f: [[fallthrough]];
    case opcode::l2d: [[fallthrough]];
    case opcode::f2i: [[fallthrough]];
    case opcode::d2l: [[fallthrough]];
    case opcode::ret: [[fallthrough]];
    case opcode::iret: [[fallthrough]];
    case opcode::lret: [[fallthrough]];
    case opcode::fret: [[fallthrough]];
    case opcode::dret: [[fallthrough]];
    case opcode::aret: [[fallthrough]];
    case opcode::i2l: [[fallthrough]];
    case opcode::i2d: [[fallthrough]];
    case opcode::f2l: [[fallthrough]];
    case opcode::f2d: [[fallthrough]];
    case opcode::l2i: [[fallthrough]];
    case opcode::l2f: [[fallthrough]];
    case opcode::d2i: [[fallthrough]];
    case opcode::d2f:
        break;
    /* opcodes with one 1-byte argument. */
    case opcode::newarray:
    {
        std::uint8_t i_u8{0};
        ar & i_u8;

        break;
    }
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32{0};
        ar & i_u32;

        break;
    }
    /* opcodes with one 8-byte argument. */
    case opcode::lconst: [[fallthrough]];
    case opcode::dconst:
    {
        std::uint64_t i_u64{0};
        ar & i_u64;

        break;
    }
    /* opcodes with one VLE integer. */
    case opcode::sconst: [[fallthrough]];
    case opcode::label: [[fallthrough]];
    case opcode::jmp:
    {
        vle_int i;
        ar & i;

        break;
    }
    /* opcodes with two VLE integers. */
    case opcode::jnz:
    {
        vle_int i1;
        vle_int i2;
        ar & i1 & i2;

        break;
    }
    /* dup_x1. */
    case opcode::dup_x1:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        break;
    }
    /* dup2_x0. */
    case opcode::dup2_x0:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        break;
    }
    /* dup_x2. */
    case opcode::dup_x2:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v3;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2 & v3;

        break;
    }
    /* invoke */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        break;
    }
    /* opcodes that need to resolve a variable. */
    case opcode::iload: [[fallthrough]];
    case opcode::lload: [[fallthrough]];
    case opcode::fload: [[fallthrough]];
    case opcode::dload: [[fallthrough]];
    case opcode::aload: [[fallthrough]];
    case opcode::istore: [[fallthrough]];
    case opcode::lstore: [[fallthrough]];
    case opcode::fstore: [[fallthrough]];
    case opcode::dstore: [[fallthrough]];
    case opcode::astore:
    {
        vle_int i;
        ar & i;

        break;
    }
    /* new. */
    case opcode::new_:
    {
        vle_int i;
        ar & i;

        break;
    }
    /* anewarray. */
    case opcode::anewarray:
    {
        vle_int i;
        ar & i;

        break;
    }
    /* setfield, getfield. */
    case opcode::setfield: [[fallthrough]];
    case opcode::getfield:
    {
        vle_int struct_index;
        vle_int field_index;
        ar & struct_index & field_index;

        break;
    }
    /* checkcast */
    case opcode::checkcast:
    {
        vle_int struct_index;
        ar & struct_index;

        break;
    }
    default:
        throw interpreter_error{
          std::format(
            "Unhandled opcode '{}' in stack delta calculation.",
            to_string(op))};
    }
}

}    // namespace

namespace slang::interpreter
{

std::unordered_map<std::int64_t, std::int32_t> analyze_bytecode_stack_depths(
  const module_loader& loader,
  std::span<const std::byte> bytecode,
  const std::__1::vector<slang::module_::exported_symbol>& exports,
  const std::__1::vector<slang::module_::imported_symbol>& imports,
  const std::function<const module_loader&(const std::string&)>& resolver)
{
    std::unordered_map<std::int64_t, std::int32_t> label_depths;
    std::unordered_map<std::int64_t, std::size_t> label_pcs;
    std::unordered_map<std::size_t, std::int32_t> pc_depths;
    std::vector<std::size_t> worklist;

    memory_read_archive input{bytecode, true, std::endian::little};

    /*
     * We're analyzing using 2 passes:
     * - The first pass maps labels to bytecode offsets.
     * - The second pass builds the control flow map graph and calculates and validates
     *   the stack depth across graph edges.
     */

    // Pass 1: Build label map.
    while(input.tell() < input.size())
    {
        const auto start_pc = input.tell();

        std::byte instr{0};
        input & instr;

        // Validate instruction range.
        if(std::to_integer<unsigned int>(instr) >= std::to_underlying(opcode::opcode_count))
        {
            throw interpreter_error{
              std::format(
                "CFG: Instruction at {} out of range.",
                start_pc)};
        }

        auto op = static_cast<opcode>(instr);
        if(op == opcode::label)
        {
            vle_int label_id;
            input & label_id;

            if(!label_pcs.try_emplace(label_id.i, start_pc).second)
            {
                throw interpreter_error{
                  std::format(
                    "CFG: Duplicate label %{} found at PC {}.",
                    label_id.i,
                    start_pc)};
            }
        }
        else
        {
            skip_operands(op, input);
        }
    }

    // Pass 2: Propagate stack depths across control flow edges.

    auto enqueue_pc = [&](std::size_t target_pc, std::int32_t depth)
    {
        if(depth < 0)
        {
            throw interpreter_error{
              std::format(
                "CFG: Attempted to enqueue target PC {} with negative stack depth {}.",
                target_pc,
                depth)};
        }

        auto it = pc_depths.find(target_pc);
        if(it == pc_depths.end())
        {
            pc_depths[target_pc] = depth;
            worklist.push_back(target_pc);
        }
        else if(it->second != depth)
        {
            throw interpreter_error{
              std::format(
                "CFG: Stack Depth Mismatch at target PC {}: path A expects {} bytes, path B provided {} bytes.",
                target_pc,
                it->second,
                depth)};
        }
    };

    auto enqueue_label = [&](std::int64_t label_id, std::int32_t depth)
    {
        const auto label_it = label_pcs.find(label_id);
        if(label_it == label_pcs.end())
        {
            throw interpreter_error{
              std::format(
                "CFG: Encountered unknown label '{}' while enqueuing.",
                label_id)};
        }

        const auto target_pc = label_it->second;
        enqueue_pc(target_pc, depth);

        auto [it, inserted] = label_depths.try_emplace(label_id, depth);
        if(!inserted && it->second != depth)
        {
            throw interpreter_error{
              std::format(
                "CFG: Stack Depth Mismatch at label %{}: path A expects {} bytes, path B provided {} bytes.",
                label_id,
                it->second,
                depth)};
        }
    };

    // Start analysis at function entry point
    enqueue_pc(0, 0);

    while(!worklist.empty())
    {
        const std::size_t cur_pc = worklist.back();
        worklist.pop_back();

        auto it = pc_depths.find(cur_pc);
        if(it == pc_depths.end())
        {
            throw interpreter_error{
              std::format(
                "CFG: Depth stack doesn't contain PC {}",
                cur_pc)};
        }

        auto cur_depth = it->second;

        input.seek(cur_pc);
        while(input.tell() < input.size())
        {
            const auto start_pc = input.tell();

            std::byte instr{0};
            input & instr;

            // Validate instruction range.
            if(std::to_integer<unsigned int>(instr) >= std::to_underlying(opcode::opcode_count))
            {
                throw interpreter_error{
                  std::format(
                    "CFG: Instruction at {} out of range.",
                    start_pc)};
            }

            auto op = static_cast<opcode>(instr);

            if(op == opcode::iret || op == opcode::lret || op == opcode::fret
               || op == opcode::dret || op == opcode::aret || op == opcode::ret)
            {
                break;
            }

            if(op == opcode::label)
            {
                vle_int label_id;
                input & label_id;

                auto [it, inserted] = label_depths.try_emplace(label_id.i, cur_depth);
                if(!inserted && it->second != cur_depth)
                {
                    throw interpreter_error{
                      std::format(
                        "CFG: Stack Depth Mismatch at label %{}: path A expects {} bytes, path B provided {} bytes.",
                        label_id.i,
                        it->second,
                        cur_depth)};
                }

                // NOTE labels don't terminate blocks, so we continue.
                continue;
            }

            if(op == opcode::jmp)
            {
                vle_int label_id;
                input & label_id;

                enqueue_label(label_id.i, cur_depth);

                break;
            }

            if(op == opcode::jnz)
            {
                vle_int then_label;
                vle_int else_label;
                input & then_label & else_label;

                if(cur_depth < jnz_condition_size)
                {
                    throw interpreter_error{
                      std::format(
                        "CFG: Stack underflow at jnz at PC {}: depth is {} bytes.",
                        start_pc,
                        cur_depth)};
                }
                const auto depth_after_pop = cur_depth - jnz_condition_size;

                // Propagate depth_after_pop to branch targets
                enqueue_label(then_label.i, depth_after_pop);
                enqueue_label(else_label.i, depth_after_pop);

                break;
            }

            // Record and validate stack delta change.
            const auto delta = get_opcode_stack_delta(
              loader,
              op,
              input,
              exports,
              imports,
              resolver);

            if(delta > 0
               && cur_depth > std::numeric_limits<std::int32_t>::max() - delta)
            {
                throw interpreter_error{
                  "CFG: Stack depth overflow."};
            }

            if(delta < 0
               && cur_depth < std::numeric_limits<std::int32_t>::min() - delta)
            {
                throw interpreter_error{
                  "CFG: Stack depth underflow."};
            }

            if(delta < 0
               && cur_depth < -delta)
            {
                throw interpreter_error{
                  std::format(
                    "CFG: Negative stack depth after '{}'.",
                    to_string(op))};
            }

            cur_depth += delta;
        }
    }

    return label_depths;
}

}    // namespace slang::interpreter
