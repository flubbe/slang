/**
 * slang - a simple scripting language.
 *
 * x86_64 instruction emitter declarations.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "registers.h"

namespace slang::jit::x86_64
{

/** x86_64 instruction emitter. */
struct instruction_emitter
{
    /** Emitted machine code. */
    std::vector<std::uint8_t> code{};

    /** Emit one byte. */
    void emit(std::uint8_t byte);

    /** Emit `MOV r64, imm64`. */
    void mov_imm64(cpu_registers destination, std::uint64_t value);

    /** Emit `MOV r32, imm32`. */
    void mov_imm32(cpu_registers destination, std::uint32_t value);

    /** Emit `MOV r64, r64`. */
    void mov_r64(cpu_registers destination, cpu_registers source);

    /** Emit `MOV r64, [base + displacement]`. */
    void load_r64(cpu_registers destination, cpu_registers base, std::int32_t displacement);

    /** Emit `MOV [base + displacement], r64`. */
    void store_r64(cpu_registers base, std::int32_t displacement, cpu_registers source);

    /** Emit `MOV r32, [base + displacement]`. */
    void load_r32(cpu_registers destination, cpu_registers base, std::int32_t displacement);

    /** Emit `MOV [base + displacement], r32`. */
    void store_r32(cpu_registers base, std::int32_t displacement, cpu_registers source);

    /** Emit `MOVSXD r64, r32`. */
    void movsxd_r64_r32(cpu_registers destination, cpu_registers source);

    /** Emit `ADD r64, r64`. */
    void add_r64(cpu_registers destination, cpu_registers source);

    /** Emit `ADD r32, r32`. */
    void add_r32(cpu_registers destination, cpu_registers source);

    /** Emit `SUB r64, r64`. */
    void sub_r64(cpu_registers destination, cpu_registers source);

    /** Emit `SUB r32, r32`. */
    void sub_r32(cpu_registers destination, cpu_registers source);

    /** Emit signed `IMUL r64, r64`. */
    void imul_r64(cpu_registers destination, cpu_registers source);

    /** Emit signed `IMUL r32, r32`. */
    void imul_r32(cpu_registers destination, cpu_registers source);

    /** Emit signed negation of a 32-bit register. */
    void neg_r32(cpu_registers destination);

    /** Emit signed negation of a 64-bit register. */
    void neg_r64(cpu_registers destination);

    /** Emit bitwise `AND` for 32-bit registers. */
    void and_r32(cpu_registers destination, cpu_registers source);

    /** Emit bitwise `AND` for 64-bit registers. */
    void and_r64(cpu_registers destination, cpu_registers source);

    /** Emit bitwise `OR` for 32-bit registers. */
    void or_r32(cpu_registers destination, cpu_registers source);

    /** Emit bitwise `OR` for 64-bit registers. */
    void or_r64(cpu_registers destination, cpu_registers source);

    /** Emit bitwise `XOR` for 32-bit registers. */
    void xor_r32(cpu_registers destination, cpu_registers source);

    /** Emit bitwise `XOR` for 64-bit registers. */
    void xor_r64(cpu_registers destination, cpu_registers source);

    /** Emit `TEST r32, r32`. */
    void test_r32(cpu_registers lhs, cpu_registers rhs);

    /** Emit `SHL r32, CL`. */
    void shl_r32_cl(cpu_registers destination);

    /** Emit `SHL r64, CL`. */
    void shl_r64_cl(cpu_registers destination);

    /** Emit arithmetic `SAR r32, CL`. */
    void sar_r32_cl(cpu_registers destination);

    /** Emit arithmetic `SAR r64, CL`. */
    void sar_r64_cl(cpu_registers destination);

    /** Move a 32-bit general-purpose register value into an XMM register. */
    void movd_xmm_r32(simd_registers destination, cpu_registers source);

    /** Move a 64-bit general-purpose register value into an XMM register. */
    void movq_xmm_r64(simd_registers destination, cpu_registers source);

    /** Move a 32-bit XMM value into a general-purpose register. */
    void movd_r32_xmm(cpu_registers destination, simd_registers source);

    /** Move a 64-bit XMM value into a general-purpose register. */
    void movq_r64_xmm(cpu_registers destination, simd_registers source);

    /** Emit scalar single-precision `ADDSS`. */
    void addss(simd_registers destination, simd_registers source);

    /** Emit scalar single-precision `SUBSS`. */
    void subss(simd_registers destination, simd_registers source);

    /** Emit scalar single-precision `MULSS`. */
    void mulss(simd_registers destination, simd_registers source);

    /** Emit scalar single-precision `DIVSS`. */
    void divss(simd_registers destination, simd_registers source);

    /** Emit scalar double-precision `ADDSD`. */
    void addsd(simd_registers destination, simd_registers source);

    /** Emit scalar double-precision `SUBSD`. */
    void subsd(simd_registers destination, simd_registers source);

    /** Emit scalar double-precision `MULSD`. */
    void mulsd(simd_registers destination, simd_registers source);

    /** Emit scalar double-precision `DIVSD`. */
    void divsd(simd_registers destination, simd_registers source);

    /** Emit `PUSH r64`. */
    void push(cpu_registers source);

    /** Emit `POP r64`. */
    void pop(cpu_registers destination);

    /** Emit `RET`. */
    void ret();

    /** Emit an indirect `CALL r64`. */
    void call_reg(cpu_registers target);

    /** Emit a near unconditional branch with a placeholder displacement. */
    void jmp_rel32();

    /** Emit a near conditional branch with a placeholder displacement. */
    void jcc_rel32(condition_code condition);

    /** Patch the displacement of an `E9 rel32` branch. */
    void patch_jmp_rel32(std::size_t instruction_index, std::size_t target_index);

    /** Patch the displacement of a `0F 8? rel32` branch. */
    void patch_jcc_rel32(std::size_t instruction_index, std::size_t target_index);
};

}    // namespace slang::jit::x86_64
