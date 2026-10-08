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

// NOLINTBEGIN(readability-magic-numbers)

namespace slang::jit::aarch64
{

void instruction_emitter::emit(
  std::uint32_t insn)
{
    code.push_back(insn);
}

void instruction_emitter::emit_ldp_stp_x(
  bool is_load,
  cpu_registers rt,
  cpu_registers rt2,
  cpu_registers rn,
  std::int32_t byte_offset,
  bool pre_index)
{
    uint32_t op = 0xA8000000;    // Base opcode mask for pair loads/stores

    if(is_load)
    {
        op |= (1u << 22u);    // L bit = 1 for LDP
    }

    if(pre_index)
    {
        op |= (3u << 23u);    // Pre-indexed: [rn, #imm]!
    }
    else
    {
        op |= (1u << 23u);    // Post-indexed: [rn], #imm
    }

    // Convert byte offset to 8-byte element count and mask to 7 bits
    std::uint32_t imm7 =
      (static_cast<std::uint32_t>(byte_offset) >> 3u) & 0x7Fu;

    op |= (imm7 << 15u);
    op |= (rt2 << 10);
    op |= (rn << 5);
    op |= rt;

    emit(op);
}

void instruction_emitter::stp_x_pre(
  cpu_registers rt,
  cpu_registers rt2,
  cpu_registers rn,
  std::int32_t offset)
{
    emit_ldp_stp_x(
      false,
      rt,
      rt2,
      rn,
      offset,
      true);
}

void instruction_emitter::ldp_x_post(
  cpu_registers rt,
  cpu_registers rt2,
  cpu_registers rn,
  std::int32_t offset)
{
    emit_ldp_stp_x(
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

void instruction_emitter::mov_reg_x(
  cpu_registers xd,
  cpu_registers xm)
{
    emit(
      0xAA0003E0
      | (xm << 16)
      | xd);
}

void instruction_emitter::movz(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    std::uint32_t hw = (shift >> 4u) & 0x1u;
    emit(
      0x52800000u
      | (hw << 21u)
      | (static_cast<std::uint32_t>(imm16) << 5u)
      | static_cast<std::uint32_t>(xd));
}

void instruction_emitter::movn_w(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    std::uint32_t hw = (shift >> 4u) & 0x1u;
    emit(
      0x12800000u
      | (hw << 21u)
      | (static_cast<std::uint32_t>(imm16) << 5u)
      | static_cast<std::uint32_t>(xd));
}

void instruction_emitter::movk_w(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    std::uint32_t hw = (shift >> 4u) & 0x1u;
    emit(
      0x72800000u
      | (hw << 21u)
      | (static_cast<std::uint32_t>(imm16) << 5u)
      | static_cast<std::uint32_t>(xd));
}

void instruction_emitter::movk_x(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    std::uint32_t hw = (shift >> 4u) & 0x3u;
    emit(
      0xf2800000
      | (hw << 21u)
      | (static_cast<std::uint32_t>(imm16) << 5u)
      | static_cast<std::uint32_t>(xd));
}

void instruction_emitter::mov_w(
  cpu_registers xd,
  std::int32_t val)
{
    auto uval = static_cast<std::uint32_t>(val);
    auto low16 = static_cast<std::uint16_t>(uval & 0xFFFFu);
    auto high16 = static_cast<std::uint16_t>((uval >> 16u) & 0xFFFFu);

    // Case 1: Fits in single MOVZ (0x0000XXXX)
    if(high16 == 0)
    {
        movz(xd, low16, 0);    // hw = 0
    }
    // Case 2: Negative/inverted that fits in single MOVN
    else if(low16 == 0xFFFF)
    {
        movn_w(xd, static_cast<std::uint16_t>(~high16), 16);    // hw = 1
    }
    // Case 3: Requires 2 instructions (MOVZ + MOVK)
    else
    {
        movz(xd, low16, 0);        // Load lower 16 bits
        movk_w(xd, high16, 16);    // Overwrite upper 16 bits with LSL #16
    }
}

void instruction_emitter::ldr_w(
  cpu_registers xd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes >> 2u) & 0xFFFu;
    emit(
      0xB9400000
      | (imm12 << 10u)
      | (xn << 5u)
      | xd);
}

void instruction_emitter::ldr_x(
  cpu_registers xd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes >> 3u) & 0xFFFu;
    emit(
      0xF9400000
      | (imm12 << 10u)
      | (xn << 5u)
      | xd);
}

void instruction_emitter::str_w(
  cpu_registers xd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    std::uint32_t imm12 = (offset_bytes >> 2u) & 0xFFFu;
    emit(
      0xB9000000
      | (imm12 << 10u)
      | (xn << 5u)
      | xd);
}

void instruction_emitter::add_w(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      0x0B000000u
      | (xm << 16)
      | (xn << 5)
      | xd);
}

void instruction_emitter::sub_w(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      0x4B000000u
      | (xm << 16)
      | (xn << 5)
      | xd);
}

void instruction_emitter::ret()
{
    emit(0xD65F03C0);
}

}    // namespace slang::jit::aarch64

// NOLINTEND(readability-magic-numbers)
