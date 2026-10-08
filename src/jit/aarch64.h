/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "aarch64/registers.h"
#include "common.h"

namespace slang::jit::aarch64
{

/**
 * AArch64 instruction emitter.
 *
 * Reference:
 *     Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile,
 *     https://support.arm.com/documentation/ddi0487/latest/
 */
struct instruction_emitter
{
    std::vector<std::uint32_t> code{};

    /*
     * Helpers.
     */

    /** Emit a 32-bit instruction. */
    void emit(
      std::uint32_t insn);

    /** Emit an LDP or STP instruction for X registers. */
    void emit_ldp_stp_x(
      bool is_load,
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t byte_offset,
      bool pre_index);

    /*
     * AArch64 instruction mappings.
     */

    /** Add (shifted register). Emits `ADD <Wd>, <Wn>, <Wm>`. */
    void add_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Emit a post-indexed LDP instruction for X registers. */
    void ldp_x_post(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Load register (immediate). Emits `LDR <Wt>, [<Xn|SP>{, #<pimm>}]`. */
    void ldr_w(
      cpu_registers wd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Load register (immediate). Emits `LDR <Xt>, [<Xn|SP>{, #<pimm>}]`. */
    void ldr_x(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Move wide with keep. Emits `MOVK <Wd>, #<imm>{, LSL #<shift>}`. */
    void movk_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with keep. Emits `MOVK <Xd>, #<imm>{, LSL #<shift>}`. */
    void movk_x(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with NOT. Emits `MOVN <Wd>, #<imm>{, LSL #<shift>}`. */
    void movn_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with zero. Emits `MOVZ <Wd>, #<imm>{, LSL #<shift>}`. */
    void movz_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Return from subroutine. Emits `RET`. */
    void ret();

    /** Emit a pre-indexed STP instruction for X registers. */
    void stp_x_pre(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Store register (immediate). Emits `STR <Wt>, [<Xn|SP>{, #<pimm>}]`. */
    void str_w(
      cpu_registers wd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    /** Subtract (shifted register). Emits `SUB <Wd>, <Wn>, <Wm>`. */
    void sub_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /*
     * Convenience operations.
     */

    /** Move register value. Emits `MOV  <Xd>, <Xm>`. */
    void mov_reg_x(
      cpu_registers xd,
      cpu_registers xm);

    /** Move a 32-bit immediate value into a W register. */
    void mov_w(
      cpu_registers wd,
      std::int32_t val);

    /** Store pair of registers (pre-indexed). Emits `STP X29, X30, [SP, #-16]!`. */
    void push_fp_lr();

    /** Load pair of registers (post-indexed). Emits `LDP X29, X30, [SP], #16`. */
    void pop_fp_lr();
};

/** AArch64 JIT compiler. */
class jit_compiler
{
    static jit_function allocate_executable_memory(
      const std::vector<std::uint32_t>& machine_code,
      std::size_t locals_size,
      std::size_t stack_size);

public:
    static si::stack_frame make_stack(
      std::size_t locals_size,
      std::size_t stack_size)
    {
        si::stack_frame frame{{}, locals_size, stack_size};
        frame.locals.resize(locals_size);
        frame.stack.stack.resize(stack_size);
        return frame;
    }

    static jit_function compile(
      const std::vector<std::byte>& bytecode);
};

}    // namespace slang::jit::aarch64
