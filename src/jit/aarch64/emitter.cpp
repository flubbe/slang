/**
 * slang - a simple scripting language.
 *
 * Just In Time instruction emitter for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "jit/aarch64.h"

namespace slang::jit::aarch64
{

void instruction_emitter::emit(
  std::uint32_t insn)
{
    code.push_back(insn);
}

void instruction_emitter::emit_ldp_stp_64(
  bool is_load,
  register_aarch64 rt,
  register_aarch64 rt2,
  register_aarch64 rn,
  std::int32_t byte_offset,
  bool pre_index)
{
    uint32_t op = 0xA8000000;    // Base opcode mask for pair loads/stores

    if(is_load)
    {
        op |= (1 << 22);    // L bit = 1 for LDP
    }

    if(pre_index)
    {
        op |= (3 << 23);    // Pre-indexed: [rn, #imm]!
    }
    else
    {
        op |= (1 << 23);    // Post-indexed: [rn], #imm
    }

    // Convert byte offset to 8-byte element count and mask to 7 bits
    int32_t imm7 = (byte_offset / 8) & 0x7F;

    op |= (imm7 << 15);
    op |= (rt2 << 10);
    op |= (rn << 5);
    op |= rt;

    emit(op);
}

void instruction_emitter::stp_x_pre(
  register_aarch64 rt,
  register_aarch64 rt2,
  register_aarch64 rn,
  std::int32_t offset)
{
    emit_ldp_stp_64(
      false,
      rt,
      rt2,
      rn,
      offset,
      true);
}

void instruction_emitter::ldp_x_post(
  register_aarch64 rt,
  register_aarch64 rt2,
  register_aarch64 rn,
  std::int32_t offset)
{
    emit_ldp_stp_64(
      true,
      rt,
      rt2,
      rn,
      offset,
      false);
}

void instruction_emitter::push_fp_lr()
{
    emit(0xA9BF7BFD);
}

void instruction_emitter::pop_fp_lr()
{
    emit(0xA8C17BFD);
}

void instruction_emitter::mov_reg(
  register_aarch64 rd,
  register_aarch64 rn)
{
    emit(0xAA0003E0 | (rn << 16) | rd);
}

void instruction_emitter::movz(
  register_aarch64 rd,
  std::uint16_t imm16,
  std::uint32_t shift)
{
    std::uint32_t hw = (shift == 16) ? 1 : 0;
    emit(0x52800000 | (hw << 21) | (static_cast<std::uint32_t>(imm16) << 5) | static_cast<std::uint32_t>(rd));
}

void instruction_emitter::movn(
  register_aarch64 rd,
  std::uint16_t imm16,
  std::uint32_t shift)
{
    std::uint32_t hw = (shift == 16) ? 1 : 0;
    emit(0x12800000 | (hw << 21) | (static_cast<std::uint32_t>(imm16) << 5) | static_cast<std::uint32_t>(rd));
}

void instruction_emitter::movk(
  register_aarch64 rd,
  std::uint16_t imm16,
  std::uint32_t shift)
{
    std::uint32_t hw = (shift == 16) ? 1 : 0;
    emit(0x72800000 | (hw << 21) | (static_cast<std::uint32_t>(imm16) << 5) | static_cast<std::uint32_t>(rd));
}

void instruction_emitter::mov_w(
  register_aarch64 rd,
  std::int32_t val)
{
    std::uint32_t uval = static_cast<std::uint32_t>(val);
    std::uint16_t low16 = static_cast<std::uint16_t>(uval & 0xFFFF);
    std::uint16_t high16 = static_cast<std::uint16_t>((uval >> 16) & 0xFFFF);

    // Case 1: Fits in single MOVZ (0x0000XXXX)
    if(high16 == 0)
    {
        movz(rd, low16, 0);    // hw = 0
    }
    // Case 2: Negative/inverted that fits in single MOVN
    else if(low16 == 0xFFFF)
    {
        movn(rd, static_cast<std::uint16_t>(~high16), 16);    // hw = 1
    }
    // Case 3: Requires 2 instructions (MOVZ + MOVK)
    else
    {
        movz(rd, low16, 0);      // Load lower 16 bits
        movk(rd, high16, 16);    // Overwrite upper 16 bits with LSL #16
    }
}

void instruction_emitter::ldr_w(
  register_aarch64 rd,
  register_aarch64 rn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes / 4) & 0xFFF;
    emit(0xB9400000 | (imm12 << 10) | (rn << 5) | rd);
}

void instruction_emitter::ldr_x(
  register_aarch64 rd,
  register_aarch64 rn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes / 8) & 0xFFF;
    emit(0xF9400000 | (imm12 << 10) | (rn << 5) | rd);
}

void instruction_emitter::str_w(
  register_aarch64 rd,
  register_aarch64 rn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes / 4) & 0xFFF;
    emit(0xB9000000 | (imm12 << 10) | (rn << 5) | rd);
}

void instruction_emitter::add_w(
  register_aarch64 rd,
  register_aarch64 rn,
  register_aarch64 rm)
{
    emit(0x0B000000 | (rm << 16) | (rn << 5) | rd);
}

void instruction_emitter::sub_w(
  register_aarch64 rd,
  register_aarch64 rn,
  register_aarch64 rm)
{
    emit(0x4B000000 | (rm << 16) | (rn << 5) | rd);
}

void instruction_emitter::ret()
{
    emit(0xD65F03C0);
}

}    // namespace slang::jit::aarch64
