/**
 * slang - a simple scripting language.
 *
 * AArch64 instruction emitter declarations.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <vector>

#include "registers.h"

namespace slang::jit::aarch64
{

/**
 * AArch64 instruction emitter.
 *
 * References
 * ----------
 * 1. Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile,
 *    https://support.arm.com/documentation/ddi0487/latest/
 */
struct instruction_emitter
{
    /** Emitted machine code. */
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

    /** Add (shifted register). Emits `ADD <Xd>, <Xn>, <Xm>`. */
    void add_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Bitwise AND (shifted register). Emits `AND <Wd>, <Wn>, <Wm>`. */
    void and_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise AND (shifted register). Emits `AND <Xd>, <Xn>, <Xm>`. */
    void and_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Arithmetic shift right variable. Emites `ASRV  <Wd>, <Wn>, <Wm>`. */
    void asrv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Arithmetic shift right variable. Emites `ASRV  <Xd>, <Xn>, <Xm>`. */
    void asrv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /**
     * Branch.
     *
     * Emits `B #<imm26>`, where the signed 26-bit immediate specifies
     * a PC-relative byte offset divided by four.
     *
     * @param byte_offset Signed byte offset from the address of this
     *     instruction to the branch target. Must be 4-byte aligned and in
     *     the range `[-2^27, 2^27)`.
     */
    void b(
      std::int32_t byte_offset);

    /** Branch with link to register. Emits `BLR <Xn>`. */
    void blr(
      cpu_registers xn);

    /**
     * Compare and branch if nonzero (32-bit).
     *
     * Emits `CBNZ <Wt>, #<imm19>`, where the signed 19-bit immediate
     * specifies a PC-relative byte offset divided by four.
     *
     * @param wt The W register to test.
     * @param byte_offset Signed byte offset from the address of this
     *     instruction to the branch target. Must be 4-byte aligned and in
     *     the range `[-2^20, 2^20)`.
     */
    void cbnz_w(
      cpu_registers wt,
      std::int32_t byte_offset);

    /** Bitwise exclusive-OR (shifted register). Emits `EOR <Wd>, <Wn>, <Wm>`. */
    void eor_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise exclusive-OR (shifted register). Emits `EOR <Xd>, <Xn>, <Xm>`. */
    void eor_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Load pair of registers (post-indexed). Emits `LDP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>`. */
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

    /** Logical shift left variable. Emits `LSLV <Wd>, <Wn>, <Wm>`. */
    void lslv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Logical shift left variable. Emits `LSLV <Xd>, <Xn>, <Xm>`. */
    void lslv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Logical shift right variable. Emits `LSRV <Wd>, <Wn>, <Wm>`. */
    void lsrv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Logical shift right variable. Emits `LSRV <Xd>, <Xn>, <Xm>`. */
    void lsrv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

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

    /** Move wide with zero. Emits `MOVZ <Xd>, #<imm>{, LSL #<shift>}`. */
    void movz_x(
      cpu_registers xd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    /** Multiply-subtract. Emits `MSUB <Wd>, <Wn>, <Wm>, <Wa>`. */
    void msub_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm,
      cpu_registers wa);

    /** Multiply-subtract. Emits `MSUB <Xd>, <Xn>, <Xm>, <Xa>`. */
    void msub_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm,
      cpu_registers xa);

    /** Multiply. Emits `MUL <Wd>, <Wn>, <Wm>`. */
    void mul_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Multiply. Emits `MUL <Xd>, <Xn>, <Xm>`. */
    void mul_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Bitwise OR (shifted register). Emits `ORR <Wd>, <Wn>, <Wm>`. */
    void orr_reg_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Bitwise OR (shifted register). Emits `ORR <Xd>, <Xn>, <Xm>`. */
    void orr_reg_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Return from subroutine. Emits `RET`. */
    void ret();

    /** Signed divide. Emits `SDIV <Wd>, <Wn>, <Wm>`. */
    void sdiv_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Signed divide. Emits `SDIV <Xd>, <Xn>, <Xm>`. */
    void sdiv_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /** Store pair of registers (pre-indexed). Emits `STP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>]!`. */
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

    /** Store register (immediate). Emits `STR <Xt>, [<Xn|SP>{, #<pimm>}]`. */
    void str_x(
      cpu_registers xd,
      cpu_registers xn,
      std::uint32_t offset_bytes);

    void sxtw(
      cpu_registers xd,
      cpu_registers xn);

    /** Subtract (shifted register). Emits `SUB <Wd>, <Wn>, <Wm>`. */
    void sub_w(
      cpu_registers wd,
      cpu_registers wn,
      cpu_registers wm);

    /** Subtract (shifted register). Emits `SUB <Xd>, <Xn>, <Xm>`. */
    void sub_x(
      cpu_registers xd,
      cpu_registers xn,
      cpu_registers xm);

    /*
     * Convenience operations.
     */

    /**
     * Load a 64-bit value from a base register plus a byte offset.
     *
     * Uses an immediate-offset load when the offset is encodable;
     * otherwise, computes the effective address in a temporary register.
     */
    void load_x(
      cpu_registers reg,
      cpu_registers base,
      std::uint32_t offset);

    /**
     * Store a 64-bit value to a base register plus a byte offset.
     *
     * Uses an immediate-offset store when the offset is encodable;
     * otherwise, computes the effective address in a temporary register.
     */
    void store_x(
      cpu_registers reg,
      cpu_registers base,
      std::uint32_t offset);

    /** Move register. Emits `MOV <Xd>, <Xm>`. */
    void mov_reg_x(
      cpu_registers xd,
      cpu_registers xm);

    /** Move a 32-bit immediate value into a W register. */
    void mov_w(
      cpu_registers wd,
      std::int32_t val);

    /** Move a 64-bit immediate value into an X register. */
    void mov_x(
      cpu_registers xd,
      std::int64_t val);

    /** Store pair of registers (pre-indexed). Emits `STP X29, X30, [SP, #-16]!`. */
    void push_fp_lr();

    /** Load pair of registers (post-indexed). Emits `LDP X29, X30, [SP], #16`. */
    void pop_fp_lr();

    /*
     * Patching.
     */

    /** Patch the target of a `B` instruction. */
    void patch_b(
      std::size_t instruction_index,
      std::size_t target_index);

    /** Patch the target of a `CBNZ <Wt>` instruction. */
    void patch_cbnz_w(
      std::size_t instruction_index,
      std::size_t target_index);
};

}    // namespace slang::jit::aarch64
