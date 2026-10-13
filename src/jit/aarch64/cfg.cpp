/**
 * slang - a simple scripting language.
 *
 * Control Flow Graph analysis for JIT compiler.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cassert>
#include <utility>

#include "archives/archive.h"
#include "jit/aarch64.h"
#include "shared/opcodes.h"
#include "shared/type_class.h"
#include "cfg.h"

namespace
{

using namespace slang;
using namespace slang::jit;
using slang::jit::aarch64::stack_slot_size;

/**
 * Compute net stack delta in bytes and advance pc for any given bytecode instruction.
 *
 * @param op The opcode.
 * @param ar Input archive (bytecode).
 * @param call_targets Call targets in the current module.
 * @param import_call_targets Imported call targets.
 * @returns The stack delta after executing the instruction.
 */
std::int32_t get_opcode_stack_delta(
  opcode op,
  archive& ar,
  const std::vector<std::unique_ptr<jit_call_target>>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets)
{
    switch(op)
    {
    /* opcodes without arguments. */
    case opcode::aconst_null: [[fallthrough]];
    case opcode::adup: [[fallthrough]];
    case opcode::dup: [[fallthrough]];
    case opcode::dup2:
        return stack_slot_size;
    case opcode::pop: [[fallthrough]];
    case opcode::pop2:
    case opcode::apop:
        return -stack_slot_size;
    case opcode::arraylength:
        return 0;
    case opcode::caload: [[fallthrough]];
    case opcode::saload: [[fallthrough]];
    case opcode::iaload: [[fallthrough]];
    case opcode::faload: [[fallthrough]];
    case opcode::laload: [[fallthrough]];
    case opcode::daload: [[fallthrough]];
    case opcode::aaload:
        return -stack_slot_size;
    case opcode::castore: [[fallthrough]];
    case opcode::sastore: [[fallthrough]];
    case opcode::iastore: [[fallthrough]];
    case opcode::fastore: [[fallthrough]];
    case opcode::lastore: [[fallthrough]];
    case opcode::dastore: [[fallthrough]];
    case opcode::aastore:
        return -3 * stack_slot_size;
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
        return -stack_slot_size;
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
    case opcode::acmpne:
        return -stack_slot_size;
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
        return 0;
    /* opcodes with one 1-byte argument. */
    case opcode::newarray:
    {
        std::uint8_t i_u8{0};
        ar & i_u8;

        return 0;
    }
    /* opcodes with one 4-byte argument. */
    case opcode::iconst: [[fallthrough]];
    case opcode::fconst:
    {
        std::uint32_t i_u32{0};
        ar & i_u32;

        return stack_slot_size;
    }
    /* opcodes with one 8-byte argument. */
    case opcode::lconst: [[fallthrough]];
    case opcode::dconst:
    {
        std::uint64_t i_u64{0};
        ar & i_u64;

        return stack_slot_size;
    }
    /* opcodes with one VLE integer. */
    case opcode::sconst:
    {
        vle_int i;
        ar & i;

        return stack_slot_size;
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

        return -stack_slot_size;
    }
    /* dup_x1. */
    case opcode::dup_x1:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        return stack_slot_size;
    }
    /* dup2_x0. */
    case opcode::dup2_x0:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2;

        return 2 * stack_slot_size;
    }
    /* dup_x2. */
    case opcode::dup_x2:
    {
        // type arguments.
        type_class v1;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v2;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        type_class v3;    // initialized during serialization. // NOLINT(cppcoreguidelines-init-variables)
        ar & v1 & v2 & v3;

        return stack_slot_size;
    }
    /* invoke */
    case opcode::invoke:
    {
        vle_int i;
        ar & i;

        const jit_call_target* target = nullptr;
        if(i.i < 0)
        {
            const auto import_index = static_cast<std::size_t>(-(i.i + 1));
            if(import_index < import_call_targets.size())
            {
                target = import_call_targets.at(import_index);
            }
        }
        else
        {
            if(static_cast<std::size_t>(i.i) < call_targets.size())
            {
                target = call_targets.at(static_cast<std::size_t>(i.i)).get();
            }
        }

        if(target == nullptr)
        {
            throw jit_error{
              std::format(
                "Invalid call target index {} during stack analysis.",
                i.i)};
        }

        return utils::numeric_cast<std::int32_t>(target->return_size)
               - utils::numeric_cast<std::int32_t>(target->argument_size);
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
                     ? -stack_slot_size
                     : stack_slot_size;
        }

        if((op == opcode::lload)
           || (op == opcode::dload)
           || (op == opcode::lstore)
           || (op == opcode::dstore))
        {
            // cat2 type.
            return is_store
                     ? -stack_slot_size
                     : stack_slot_size;
        }

        // cat1 type.
        return is_store
                 ? -stack_slot_size
                 : stack_slot_size;
    }
    /* new. */
    case opcode::new_:
    {
        vle_int i;
        ar & i;

        return stack_slot_size;
    }
    /* anewarray. */
    case opcode::anewarray:
    {
        vle_int i;
        ar & i;

        return 0;
    }
    /* setfield, getfield. */
    case opcode::setfield: [[fallthrough]];
    case opcode::getfield:
    {
        vle_int struct_index;
        vle_int field_index;
        ar & struct_index & field_index;

        if(op == opcode::setfield)
        {
            return -static_cast<std::int32_t>(2 * stack_slot_size);
        }
        return 0;
    }
    /* checkcast */
    case opcode::checkcast:
    {
        vle_int struct_index;
        ar & struct_index;

        return 0; /* no stack size change */
    }
    default:
        throw jit_error{
          std::format(
            "Unhandled opcode '{}' in stack delta calculation.",
            to_string(op))};
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
        throw jit_error{
          std::format(
            "Unhandled opcode '{}' in stack delta calculation.",
            to_string(op))};
    }
}

}    // namespace

namespace slang::jit
{

std::unordered_map<std::int64_t, std::int32_t> analyze_bytecode_stack_depths(
  std::span<const std::byte> bytecode,
  const std::vector<std::unique_ptr<jit_call_target>>& call_targets,
  const std::vector<jit_call_target*>& import_call_targets)
{
    std::unordered_map<std::int64_t, std::int32_t> label_depths;
    std::unordered_map<std::int64_t, std::size_t> label_pcs;
    std::unordered_map<std::size_t, std::int64_t> pcs_label;
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
        const auto pc_start = input.tell();

        std::byte instr{0};
        input & instr;

        auto op = static_cast<opcode>(instr);
        if(op == opcode::label)
        {
            vle_int label_id;
            input & label_id;

            label_pcs[label_id.i] = pc_start;
            pcs_label[pc_start] = label_id.i;
        }
        else
        {
            skip_operands(op, input);
        }
    }

    // Pass 2: Propagate stack depths across control flow edges.

    auto enqueue = [&](std::size_t target_pc, std::int32_t depth)
    {
        if(depth < 0)
        {
            throw jit_error{
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
            throw jit_error{
              std::format(
                "CFG: Stack Depth Mismatch at target PC {}: path A expects {} bytes, path B provided {} bytes.",
                target_pc,
                it->second,
                depth)};
        }
    };

    // Start analysis at function entry point
    enqueue(0, 0);

    while(!worklist.empty())
    {
        const std::size_t cur_pc = worklist.back();
        worklist.pop_back();

        auto it = pc_depths.find(cur_pc);
        if(it == pc_depths.end())
        {
            throw jit_error{
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
                throw jit_error{
                  std::format(
                    "CFG: Instruction at {} out of range.",
                    start_pc)};
            }

            auto op = static_cast<opcode>(instr);

            if(op == opcode::label)
            {
                vle_int label_id;
                input & label_id;

                // If this label was already enqueued by a jump/branch, verify depth consistency
                auto it = pc_depths.find(start_pc);
                if(it != pc_depths.end())
                {
                    if(it->second != cur_depth)
                    {
                        throw jit_error{
                          std::format(
                            "CFG Stack Depth Mismatch at label {} (PC {}): fall-through expects {} bytes, path provided {} bytes.",
                            label_id.i,
                            start_pc,
                            it->second,
                            cur_depth)};
                    }
                }
                else
                {
                    // Fall-through into new label: record depth for this PC
                    enqueue(start_pc, cur_depth);
                }

                label_depths[label_id.i] = cur_depth;

                continue;
            }

            if(op == opcode::jmp)
            {
                vle_int label_id;
                input & label_id;

                label_depths[label_id.i] = cur_depth;
                if(auto it = label_pcs.find(label_id.i); it != label_pcs.end())
                {
                    enqueue(it->second, cur_depth);
                }
                break;
            }

            if(op == opcode::jnz)
            {
                vle_int then_label;
                vle_int else_label;
                input & then_label & else_label;

                if(cur_depth < stack_slot_size)
                {
                    throw jit_error{
                      std::format(
                        "CFG: Stack underflow at jnz at PC {}: depth is {} bytes.",
                        start_pc,
                        cur_depth)};
                }
                const auto depth_after_pop = cur_depth - stack_slot_size;

                // Propagate depth_after_pop to branch targets
                label_depths[then_label.i] = depth_after_pop;
                label_depths[else_label.i] = depth_after_pop;

                if(auto it = label_pcs.find(then_label.i); it != label_pcs.end())
                {
                    enqueue(it->second, depth_after_pop);
                }
                if(auto it = label_pcs.find(else_label.i); it != label_pcs.end())
                {
                    enqueue(it->second, depth_after_pop);
                }
                break;
            }

            if(op == opcode::iret || op == opcode::lret || op == opcode::fret
               || op == opcode::dret || op == opcode::aret || op == opcode::ret)
            {
                break;
            }

            const auto delta = get_opcode_stack_delta(
              op,
              input,
              call_targets,
              import_call_targets);
            cur_depth += delta;

            if(cur_depth < 0)
            {
                throw jit_error{
                  std::format(
                    "CFG: Negative stack depth after '{}'.",
                    to_string(op))};
            }
        }
    }

    return label_depths;
}

}    // namespace slang::jit