/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <sys/mman.h>

#include "jit/aarch64.h"

namespace si = slang::interpreter;

namespace
{

/** X register size, as `std::int32_t` to avoid some type conversions. */
constexpr auto x_register_size = static_cast<std::int32_t>(sizeof(std::uint64_t));

/** Struct helper holding offsets of the stack frame elements. */
struct stack_frame_offsets
{
    /** Offset of the locals. */
    std::size_t locals;

    /** Offset of the stack. */
    std::size_t stack;
};

/** Calculate the stack frame offsets. */
stack_frame_offsets calculate_stack_frame_offsets()
{
    const std::vector<slang::module_::constant_table_entry> dummy_constants;
    const si::stack_frame dummy_frame{dummy_constants, 0, 0};

    const auto base =
      reinterpret_cast<std::uintptr_t>(&dummy_frame);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t locals_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.locals) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t stack_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.stack) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return {
      .locals = locals_offset,
      .stack = stack_offset,
    };
}

}    // namespace

namespace slang::jit::aarch64
{

jit_function jit_compiler::allocate_executable_memory(
  const std::vector<std::uint32_t>& machine_code,
  std::size_t locals_size,
  std::size_t stack_size)
{
    executable_memory memory{machine_code};

    auto function =
      reinterpret_cast<jit_function_pointer>(memory.data());    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return jit_function{
      std::move(memory),
      function,
      locals_size,
      stack_size};
}

jit_function jit_compiler::compile(
  const std::vector<std::byte>& bytecode)
{
    /*
     * Register Mapping
     * ----------------
     * X0  = Pointer to stack_frame passed at runtime
     * X19 = Direct pointer to frame->locals.data()
     * X20 = Direct pointer to frame->stack.data()
     */

    instruction_emitter e;

    /*
     * Prologue.
     */

    e.push_fp_lr();

    // Save callee-saved registers X19, X20 to stack
    e.stp_x_pre(
      cpu_registers::X19,
      cpu_registers::X20,
      cpu_registers::SP,
      -2 * x_register_size);

    // Get dynamic offsets regardless of non-standard layout rules
    const auto [locals_offset, stack_offset] = calculate_stack_frame_offsets();

    // The offsets should always be smaller, otherwise something is wrong.
    assert(locals_offset < std::numeric_limits<std::uint32_t>::max());
    assert(stack_offset < std::numeric_limits<std::uint32_t>::max());

    // Load frame->locals.data() into X19
    e.ldr_x(
      cpu_registers::X19,
      cpu_registers::X0,
      static_cast<std::uint32_t>(locals_offset));

    // Load frame->stack.data() into X20
    e.ldr_x(
      cpu_registers::X20,
      cpu_registers::X0,
      static_cast<std::uint32_t>(stack_offset));

    /*
     * Compile bytecode.
     */

    std::size_t pc = 0;

    std::uint32_t max_stack_size{0};
    std::uint32_t current_stack_size{0};

    const auto update_stack_size =
      [&](opcode instr, std::int32_t delta)
    {
        if(delta == 0)
        {
            return;
        }

        if(delta < 0
           && current_stack_size < static_cast<std::uint32_t>(-delta))
        {
            throw jit_error{
              std::format(
                "Got negative stack size while decoding instruction '{}'.",
                to_string(instr))};
        }

        current_stack_size += delta;
        max_stack_size = std::max(max_stack_size, current_stack_size);
    };

    while(pc < bytecode.size())
    {
        auto op = static_cast<opcode>(bytecode.at(pc++));

        switch(op)
        {
        case opcode::iconst:
        {
            std::int32_t val{0};
            std::memcpy(
              &val,
              &bytecode.at(pc),
              sizeof(val));

            pc += sizeof(val);

            auto uval = static_cast<std::uint32_t>(val);
            auto low16 = static_cast<std::uint16_t>(uval & 0xFFFFu);              // NOLINT(readability-magic-numbers)
            auto high16 = static_cast<std::uint16_t>((uval >> 16u) & 0xFFFFu);    // NOLINT(readability-magic-numbers)

            // Always emit MOVZ (low 16 bits) + MOVK (high 16 bits if non-zero)
            e.movz_w(
              cpu_registers::X0,
              low16,
              0);
            if(high16 != 0)
            {
                e.movk_w(
                  cpu_registers::X0,
                  high16,
                  16);    // Shift 16 bits left // NOLINT(readability-magic-numbers)
            }

            e.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);

            update_stack_size(op, 4);

            break;
        }
        case opcode::iload:
        {
            int64_t local_idx{0};
            std::memcpy(
              &local_idx,
              &bytecode.at(pc),
              sizeof(local_idx));

            pc += sizeof(local_idx);

            // Read from X19 (locals), push to X20 (stack)
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X19,
              static_cast<std::uint32_t>(local_idx));
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);

            update_stack_size(op, 4);

            break;
        }
        case opcode::istore:
        {
            int64_t local_idx{0};
            std::memcpy(
              &local_idx,
              &bytecode.at(pc),
              sizeof(local_idx));

            pc += sizeof(local_idx);
            update_stack_size(op, -4);

            // Pop from X20 (stack), write to X19 (locals)
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size);
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X19,
              static_cast<std::uint32_t>(local_idx));

            break;
        }
        case opcode::iadd:
        {
            e.ldr_w(
              cpu_registers::X1,
              cpu_registers::X20,
              current_stack_size - 4);    // RHS
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size - 8);    // LHS // NOLINT(readability-magic-numbers)
            e.add_w(
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size - 8);    // NOLINT(readability-magic-numbers)

            update_stack_size(op, -4);

            break;
        }
        case opcode::isub:
        {
            e.ldr_w(
              cpu_registers::X1,
              cpu_registers::X20,
              current_stack_size - 4);    // RHS
            e.ldr_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size - 8);    // LHS // NOLINT(readability-magic-numbers)
            e.sub_w(
              cpu_registers::X0,
              cpu_registers::X0,
              cpu_registers::X1);
            e.str_w(
              cpu_registers::X0,
              cpu_registers::X20,
              current_stack_size - 8);    // NOLINT(readability-magic-numbers)

            update_stack_size(op, -4);

            break;
        }
        case opcode::ret:
        {
            // Epilogue: restore callee-saved registers

            // Pop X19 and X20
            e.ldp_x_post(
              cpu_registers::X19,
              cpu_registers::X20,
              cpu_registers::SP,
              2 * x_register_size);

            e.pop_fp_lr();
            e.ret();
            break;
        }
        default:
            throw std::runtime_error{"Unimplemented opcode."};
        }
    }

    return allocate_executable_memory(
      e.code,
      0 /* TODO */,
      max_stack_size);
}

}    // namespace slang::jit::aarch64
