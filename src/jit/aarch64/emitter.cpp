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

namespace
{

/** Encode the destination and three source/register fields. */
std::uint32_t encode_register_fields(
  std::uint32_t base,
  cpu_registers rd,
  cpu_registers rn,
  cpu_registers rm)
{
    return base | (rm << 16) | (rn << 5) | rd;
}

/** Encode the destination and three source/register fields, including Ra. */
std::uint32_t encode_register_fields(
  std::uint32_t base,
  cpu_registers rd,
  cpu_registers rn,
  cpu_registers rm,
  cpu_registers ra)
{
    return encode_register_fields(base, rd, rn, rm) | (ra << 10);
}

/** Encode an unsigned 12-bit immediate offset field. */
std::uint32_t encode_unsigned_offset_fields(
  std::uint32_t base,
  cpu_registers rt,
  cpu_registers rn,
  std::uint32_t imm12)
{
    return base | (imm12 << 10u) | (rn << 5u) | rt;
}

/** Represents a 16-bit immediate and its halfword shift for a wide-immediate instruction. */
struct wide_immediate_operand
{
    /** 16-bit immediate value. */
    std::uint16_t imm16;

    /** Left shift amount in bits. */
    std::uint32_t shift_amount;

    /** Mask applied to the encoded halfword selector. */
    std::uint32_t hw_mask;
};

/** Encode the destination register and immediate fields of a wide-immediate instruction. */
std::uint32_t encode_wide_immediate_fields(
  std::uint32_t base,
  cpu_registers rd,
  wide_immediate_operand operand)
{
    const std::uint32_t hw = (operand.shift_amount >> 4u) & operand.hw_mask;
    return base
           | (hw << 21u)
           | (static_cast<std::uint32_t>(operand.imm16) << 5u)
           | rd;
}

}    // namespace

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
    if((byte_offset % 8) != 0)
    {
        throw jit_error{
          "LDP/STP X offset must be a multiple of 8"};
    }

    if(byte_offset < -512 || byte_offset > 504)
    {
        throw jit_error{
          "LDP/STP X offset is out of range"};
    }

    uint32_t op = 0xA8000000u;    // Base opcode mask for pair loads/stores

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
    op |= (rt2 << 10u);
    op |= (rn << 5u);
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
      encode_register_fields(0x0B000000u, wd, wn, wm));
}

void instruction_emitter::add_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x8B000000u, xd, xn, xm));
}

void instruction_emitter::and_reg_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x0A000000u, wd, wn, wm));
}

void instruction_emitter::and_reg_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x8A000000u, xd, xn, xm));
}

void instruction_emitter::b(
  std::int32_t byte_offset)
{
    // byte_offset is encoded into <imm26> as byte_offset = <imm26>*4.

    if((byte_offset % 4) != 0
       || byte_offset < -(1 << 27)     // NOLINT(bugprone-signed-bitwise)
       || byte_offset >= (1 << 27))    // NOLINT(bugprone-signed-bitwise)
    {
        throw jit_error{
          "B branch offset is out of range or unaligned."};
    }

    byte_offset /= 4;    // use division instead of shift because of sign

    emit(0x14000000u | (static_cast<std::uint32_t>(byte_offset) & 0x03FFFFFFu));
}

void instruction_emitter::blr(
  cpu_registers xn)
{
    emit(0xD63F0000u | (xn << 5u));
}

void instruction_emitter::cbnz_w(
  cpu_registers wt,
  std::int32_t byte_offset)
{
    // byte_offset is encoded into <imm19> as byte_offset = <imm19>*4.

    if((byte_offset % 4) != 0
       || byte_offset < -(1 << 20)     // NOLINT(bugprone-signed-bitwise)
       || byte_offset >= (1 << 20))    // NOLINT(bugprone-signed-bitwise)
    {
        throw jit_error{
          "CBNZ branch offset is out of range or unaligned."};
    }
    byte_offset /= 4;    // use division instead of shift because of sign

    emit(
      0x35000000u
      | ((static_cast<std::uint32_t>(byte_offset) & 0x7FFFFu) << 5u)
      | static_cast<std::uint32_t>(wt));
}

void instruction_emitter::eor_reg_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x4A000000u, wd, wn, wm));
}

void instruction_emitter::eor_reg_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0xCA000000u, xd, xn, xm));
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

    const std::uint32_t imm12 = offset_bytes >> 2u;
    emit(
      encode_unsigned_offset_fields(0xB9400000u, wd, xn, imm12));
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

    const std::uint32_t imm12 = offset_bytes >> 3u;
    emit(
      encode_unsigned_offset_fields(0xF9400000u, xd, xn, imm12));
}

void instruction_emitter::lslv_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x1AC02000u, wd, wn, wm));
}

void instruction_emitter::lslv_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x9AC02000u, xd, xn, xm));
}

void instruction_emitter::lsrv_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x1AC02400u, wd, wn, wm));
}

void instruction_emitter::lsrv_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x9AC02400u, xd, xn, xm));
}

void instruction_emitter::movk_w(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    if(shift != 0 && shift != 16)
    {
        throw jit_error{
          "MOVK W shift must be 0 or 16"};
    }

    emit(encode_wide_immediate_fields(
      0x72800000u,
      xd,
      {.imm16 = imm16, .shift_amount = shift, .hw_mask = 0x1u}));
}

void instruction_emitter::movk_x(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    if(shift != 0 && shift != 16 && shift != 32 && shift != 48)
    {
        throw jit_error{
          "MOVK X shift must be 0, 16, 32 or 48"};
    }

    emit(encode_wide_immediate_fields(
      0xF2800000u,
      xd,
      {.imm16 = imm16, .shift_amount = shift, .hw_mask = 0x3u}));
}

void instruction_emitter::movn_w(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    if(shift != 0 && shift != 16)
    {
        throw jit_error{
          "MOVN W shift must be 0 or 16"};
    }

    emit(encode_wide_immediate_fields(
      0x12800000u,
      xd,
      {.imm16 = imm16, .shift_amount = shift, .hw_mask = 0x1u}));
}

void instruction_emitter::movz_w(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    if(shift != 0 && shift != 16)
    {
        throw jit_error{
          "MOVZ W shift must be 0 or 16"};
    }

    emit(encode_wide_immediate_fields(
      0x52800000u,
      xd,
      {.imm16 = imm16, .shift_amount = shift, .hw_mask = 0x1u}));
}

void instruction_emitter::movz_x(
  cpu_registers xd,
  std::uint16_t imm16,    // NOLINT(bugprone-easily-swappable-parameters)
  std::uint32_t shift)
{
    if(shift != 0 && shift != 16 && shift != 32 && shift != 48)
    {
        throw jit_error{
          "MOVZ X shift must be 0, 16, 32 or 48"};
    }

    emit(encode_wide_immediate_fields(
      0xD2800000u,
      xd,
      {.imm16 = imm16, .shift_amount = shift, .hw_mask = 0x3u}));
}

void instruction_emitter::msub_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm,
  cpu_registers wa)
{
    emit(
      encode_register_fields(0x1B008000u, wd, wn, wm, wa));
}

void instruction_emitter::msub_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm,
  cpu_registers xa)
{
    emit(
      encode_register_fields(0x9B008000u, xd, xn, xm, xa));
}

void instruction_emitter::mul_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x1B007C00u, wd, wn, wm));
}

void instruction_emitter::mul_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x9B007C00u, xd, xn, xm));
}

void instruction_emitter::orr_reg_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x2A000000u, wd, wn, wm));
}

void instruction_emitter::orr_reg_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0xAA000000u, xd, xn, xm));
}

void instruction_emitter::ret()
{
    emit(0xD65F03C0);
}

void instruction_emitter::sdiv_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x1AC00C00u, wd, wn, wm));
}

void instruction_emitter::sdiv_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0x9AC00C00u, xd, xn, xm));
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

    const std::uint32_t imm12 = offset_bytes >> 2u;
    emit(
      encode_unsigned_offset_fields(0xB9000000u, wd, xn, imm12));
}

void instruction_emitter::str_x(
  cpu_registers xd,
  cpu_registers xn,
  std::uint32_t offset_bytes)
{
    if((offset_bytes & 0x7u) != 0)
    {
        throw jit_error{"STR X offset must be a multiple of 8"};
    }
    if(offset_bytes > 0xFFFu * 8u)
    {
        throw jit_error{"STR X offset is out of range"};
    }

    const std::uint32_t imm12 = offset_bytes >> 3u;
    emit(encode_unsigned_offset_fields(0xF9000000u, xd, xn, imm12));
}

void instruction_emitter::sub_w(
  cpu_registers wd,
  cpu_registers wn,
  cpu_registers wm)
{
    emit(
      encode_register_fields(0x4B000000u, wd, wn, wm));
}

void instruction_emitter::sub_x(
  cpu_registers xd,
  cpu_registers xn,
  cpu_registers xm)
{
    emit(
      encode_register_fields(0xCB000000u, xd, xn, xm));
}

/*
 * Convenience operations.
 */

void instruction_emitter::mov_reg_x(
  cpu_registers xd,
  cpu_registers xm)
{
    emit(
      encode_register_fields(
        0xAA000000u,
        xd,
        cpu_registers::XZR,
        xm));
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

void instruction_emitter::mov_x(
  cpu_registers xd,
  std::int64_t val)
{
    auto bits = static_cast<std::uint64_t>(val);
    movz_x(xd, static_cast<std::uint16_t>(bits & 0xFFFFu));
    for(std::uint32_t shift = 16; shift < 64; shift += 16)
    {
        auto imm16 = static_cast<std::uint16_t>((bits >> shift) & 0xFFFFu);
        if(imm16 != 0)
        {
            movk_x(xd, imm16, shift);
        }
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
