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

    /** Emit a 32-bit instruction. */
    void emit(
      std::uint32_t insn);

    /** Emit an STP or LDP instruction for X registers. */
    void emit_ldp_stp_x(
      bool is_load,
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t byte_offset,
      bool pre_index);

    /** Emit a pre-indexed STP instruction for X registers. */
    void stp_x_pre(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Emit a post-indexed LDP instruction for X registers. */
    void ldp_x_post(
      cpu_registers rt,
      cpu_registers rt2,
      cpu_registers rn,
      std::int32_t offset);

    /** Push frame pointer and link register. Emits `STP x29, x30, [sp, #-16]!`. */
    void push_fp_lr();

    /** Pop frame pointer and link register. Emits `LDP x29, x30, [sp], #16`. */
    void pop_fp_lr();

    /** Move register value. Emits `MOV Xd, Xm`. */
    void mov_reg_x(
      cpu_registers xd,
      cpu_registers xm);

    /**
     * Move wide with NOT. Emits `MOVN w_rd, #imm16, LSL #hw`.
     *
     * `shift` is either 0 (the default), 16, 32, or 48, encoded in the  `hw` field as `<shift>/16`.
     * */
    void movn_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Move wide with zero. Emits `MOVZ w_rd, #imm16, LSL #hw` (`hw` is 0 or 16). */
    void movz(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    // MOVK w_rd, #imm16, LSL #hw
    void movk_w(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    // MOVK x_rd, #imm16, LSL #hw
    void movk_x(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    void mov_w(
      cpu_registers xd,
      std::int32_t val);

    // LDR w0, [rn, #offset]  (Load 32-bit word, unsigned offset)
    void ldr_w(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    // Add 64-bit LDR instruction helper for AArch64
    void ldr_x(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    // STR w0, [rn, #offset]  (Store 32-bit word, unsigned offset)
    void str_w(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    // ADD w_rd, w_rn, w_rm
    void add_w(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    // SUB w_rd, w_rn, w_rm
    void sub_w(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    // RET
    void ret();
};

/** AArch64 JIT compiler. */
class jit_compiler
{
    static jit_function allocate_executable_memory(
      const std::vector<std::uint32_t>& machine_code);

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
