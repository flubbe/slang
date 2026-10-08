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

/*
 * Helpers.
 */

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

/*
 * AArch64 instruction mappings.
 */

void instruction_emitter::add_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      0x0B000000u
      | (wm << 16)
      | (wn << 5)
      | wd);
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

void instruction_emitter::ldr_w(
  cpu_registers wd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    if((offset_bytes & 0x3u) != 0)
    {
        throw jit_error{
          "LDR W offset must be a multiple of 4"};
    }

    if(offset_bytes > 0xFFFu * 4u)
    {
        throw jit_error{
          "LDR W offset is out of range"};
    }

    std::uint32_t imm12 = offset_bytes >> 2u;
    emit(
      0xB9400000
      | (imm12 << 10u)
      | (xn << 5u)
      | wd);
}

void instruction_emitter::ldr_x(
  cpu_registers xd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    if((offset_bytes & 0x7u) != 0)
    {
        throw jit_error{
          "LDR X offset must be a multiple of 8"};
    }

    if(offset_bytes > 0xFFFu * 8u)
    {
        throw jit_error{
          "LDR X offset is out of range"};
    }

    std::uint32_t imm12 = offset_bytes >> 3u;
    emit(
      0xF9400000
      | (imm12 << 10u)
      | (xn << 5u)
      | xd);
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

void instruction_emitter::movz_w(
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

void instruction_emitter::ret()
{
    emit(0xD65F03C0);
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

void instruction_emitter::str_w(
  cpu_registers wd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    if((offset_bytes & 0x3u) != 0)
    {
        throw jit_error{
          "STR W offset must be a multiple of 4"};
    }

    if(offset_bytes > 0xFFFu * 4u)
    {
        throw jit_error{
          "STR W offset is out of range"};
    }

    std::uint32_t imm12 = offset_bytes >> 2u;
    emit(
      0xB9000000
      | (imm12 << 10u)
      | (xn << 5u)
      | wd);
}

void instruction_emitter::sub_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      0x4B000000u
      | (wm << 16)
      | (wn << 5)
      | wd);
}

/*
 * Convenience operations.
 */

void instruction_emitter::mov_reg_x(
  cpu_registers xd,
  cpu_registers xm)
{
    emit(
      0xAA0003E0
      | (xm << 16)
      | xd);
}

void instruction_emitter::mov_w(
  cpu_registers wd,
  std::int32_t val)
{
    auto uval = static_cast<std::uint32_t>(val);
    auto low16 = static_cast<std::uint16_t>(uval & 0xFFFFu);
    auto high16 = static_cast<std::uint16_t>((uval >> 16u) & 0xFFFFu);

    // Case 1: Fits in single MOVZ (0x0000XXXX)
    if(high16 == 0)
    {
        movz_w(wd, low16, 0);    // hw = 0
    }
    // Case 2: Negative/inverted that fits in single MOVN
    else if(low16 == 0xFFFF)
    {
        movn_w(wd, static_cast<std::uint16_t>(~high16), 16);    // hw = 1
    }
    // Case 3: Requires 2 instructions (MOVZ + MOVK)
    else
    {
        movz_w(wd, low16, 0);      // Load lower 16 bits
        movk_w(wd, high16, 16);    // Overwrite upper 16 bits with LSL #16
    }
}

void instruction_emitter::push_fp_lr()
{
    emit(0xA9BF7BFD);
}

void instruction_emitter::pop_fp_lr()
{
    emit(0xA8C17BFD);
}

}    // namespace slang::jit::aarch64

// NOLINTEND(readability-magic-numbers)
