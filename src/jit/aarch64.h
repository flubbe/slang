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

namespace slang::jit
{

/** AArch64 instruction emitter. */
struct emitter_aarch64
{
    std::vector<std::uint32_t> code;

    void emit(
      std::uint32_t insn);

    // Emit STP or LDP for 64-bit registers
    void emit_ldp_stp_64(
      bool is_load,
      register_aarch64 rt,
      register_aarch64 rt2,
      register_aarch64 rn,
      std::int32_t byte_offset,
      bool pre_index);

    // Convenient helper wrappers:
    void stp_x_pre(
      register_aarch64 rt,
      register_aarch64 rt2,
      register_aarch64 rn,
      std::int32_t offset);

    void ldp_x_post(
      register_aarch64 rt,
      register_aarch64 rt2,
      register_aarch64 rn,
      std::int32_t offset);

    // STP x29, x30, [sp, #-16]!  (Push frame pointer & link register)
    void push_fp_lr();

    // LDP x29, x30, [sp], #16   (Pop frame pointer & link register)
    void pop_fp_lr();

    // MOV rd, rn
    void mov_reg(
      register_aarch64 rd,
      register_aarch64 rn);

    // MOVZ w_rd, #imm16, LSL #hw (hw is 0 or 16)
    void movz(
      register_aarch64 rd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    // MOVN w_rd, #imm16, LSL #hw
    void movn(
      register_aarch64 rd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    // MOVK w_rd, #imm16, LSL #hw
    void movk(
      register_aarch64 rd,
      std::uint16_t imm16,
      std::uint32_t shift = 0);

    void mov_w(
      register_aarch64 rd,
      std::int32_t val);

    // LDR w0, [rn, #offset]  (Load 32-bit word, unsigned offset)
    void ldr_w(
      register_aarch64 rd,
      register_aarch64 rn,
      std::uint32_t offset_bytes);

    // Add 64-bit LDR instruction helper for AArch64
    void ldr_x(
      register_aarch64 rd,
      register_aarch64 rn,
      std::uint32_t offset_bytes);

    // STR w0, [rn, #offset]  (Store 32-bit word, unsigned offset)
    void str_w(
      register_aarch64 rd,
      register_aarch64 rn,
      std::uint32_t offset_bytes);

    // ADD w_rd, w_rn, w_rm
    void add_w(
      register_aarch64 rd,
      register_aarch64 rn,
      register_aarch64 rm);

    // SUB w_rd, w_rn, w_rm
    void sub_w(
      register_aarch64 rd,
      register_aarch64 rn,
      register_aarch64 rm);

    // RET
    void ret();
};

/** AArch64 JIT compiler. */
class jit_compiler_aarch64
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

}    // namespace slang::jit
