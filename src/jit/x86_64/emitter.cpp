/**
 * slang - a simple scripting language.
 *
 * Just In Time instruction emitter for x86_64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <bit>
#include <limits>

#include "jit/common.h"
#include "emitter.h"

// NOLINTBEGIN(readability-magic-numbers)

namespace slang::jit::x86_64
{

namespace
{

std::uint8_t register_id(
  cpu_registers reg)
{
    const auto id = std::to_underlying(reg);
    if(id > 15)
    {
        throw jit_error{
          "Invalid x86_64 register."};
    }

    return id;
}

std::uint8_t register_id(
  simd_registers reg)
{
    const auto id = std::to_underlying(reg);
    if(id > 15)
    {
        throw jit_error{"Invalid x86_64 SIMD register."};
    }

    return id;
}

void emit_rex(
  instruction_emitter& emitter,
  bool wide,
  std::uint8_t reg,
  std::uint8_t base)
{
    const auto rex = static_cast<std::uint8_t>(
      0x40u
      | (wide ? 0x08u : 0u)
      | ((reg & 0x08u) != 0 ? 0x04u : 0u)
      | ((base & 0x08u) != 0 ? 0x01u : 0u));
    if(rex != 0x40u)
    {
        emitter.emit(rex);
    }
}

void emit_u32(
  instruction_emitter& emitter,
  std::uint32_t value)
{
    for(unsigned int shift = 0; shift < 32; shift += 8)
    {
        emitter.emit(
          static_cast<std::uint8_t>(value >> shift));
    }
}

void emit_memory_operand(
  instruction_emitter& emitter,
  std::uint8_t opcode,
  bool wide,
  cpu_registers register_operand,
  cpu_registers base)
{
    const auto reg = register_id(register_operand);
    const auto base_id = register_id(base);

    emit_rex(emitter, wide, reg, base_id);
    emitter.emit(opcode);
    emitter.emit(static_cast<std::uint8_t>(
      0x80u | ((reg & 0x07u) << 3u) | (base_id & 0x07u)));

    if((base_id & 0x07u) == 0x04u)
    {
        emitter.emit(
          static_cast<std::uint8_t>(0x20u | (base_id & 0x07u)));
    }
}

void emit_memory_displacement(instruction_emitter& emitter, std::int32_t displacement)
{
    emit_u32(
      emitter,
      std::bit_cast<std::uint32_t>(displacement));
}

void emit_scalar_sse(
  instruction_emitter& emitter,
  std::uint8_t prefix,
  std::uint8_t opcode,
  simd_registers destination,
  simd_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emitter.emit(prefix);
    emit_rex(emitter, false, dst, src);
    emitter.emit(0x0F);
    emitter.emit(opcode);
    emitter.emit(static_cast<std::uint8_t>(
      0xC0u | ((dst & 0x07u) << 3u) | (src & 0x07u)));
}

std::int32_t branch_displacement(
  std::size_t instruction_index,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t target_index,
  std::size_t instruction_size)
{
    if(instruction_index > std::numeric_limits<std::size_t>::max() - instruction_size)
    {
        throw jit_error{
          "x86_64 branch target is out of range."};
    }

    const auto next_instruction = instruction_index + instruction_size;
    if(target_index >= next_instruction)
    {
        const auto distance = target_index - next_instruction;
        if(distance > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw jit_error{
              "x86_64 branch target is out of range."};
        }
        return static_cast<std::int32_t>(distance);
    }

    const auto distance = next_instruction - target_index;
    constexpr auto negative_limit =
      static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) + 1;

    if(distance > negative_limit)
    {
        throw jit_error{
          "x86_64 branch target is out of range."};
    }

    if(distance == negative_limit)
    {
        return std::numeric_limits<std::int32_t>::min();
    }

    return -static_cast<std::int32_t>(distance);
}

}    // namespace

void instruction_emitter::emit(
  std::uint8_t byte)
{
    code.push_back(byte);
}

void instruction_emitter::mov_imm64(
  cpu_registers destination,
  std::uint64_t value)
{
    const auto reg = register_id(destination);
    emit_rex(*this, true, 0, reg);
    emit(static_cast<std::uint8_t>(0xB8u + (reg & 0x07u)));
    for(unsigned int shift = 0; shift < 64; shift += 8)
    {
        emit(static_cast<std::uint8_t>(value >> shift));
    }
}

void instruction_emitter::mov_imm32(
  cpu_registers destination,
  std::uint32_t value)
{
    const auto reg = register_id(destination);
    emit_rex(*this, false, 0, reg);
    emit(static_cast<std::uint8_t>(0xB8u + (reg & 0x07u)));
    emit_u32(*this, value);
}

void instruction_emitter::mov_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);
    emit_rex(*this, true, src, dst);
    emit(0x89);
    emit(static_cast<std::uint8_t>(0xC0u | ((src & 0x07u) << 3u) | (dst & 0x07u)));
}

void instruction_emitter::load_r64(
  cpu_registers destination,
  cpu_registers base,
  std::int32_t displacement)
{
    emit_memory_operand(
      *this,
      0x8B,
      true,
      destination, base);
    emit_memory_displacement(
      *this,
      displacement);
}

void instruction_emitter::store_r64(
  cpu_registers base,
  std::int32_t displacement,
  cpu_registers source)
{
    emit_memory_operand(
      *this,
      0x89,
      true,
      source,
      base);
    emit_memory_displacement(
      *this,
      displacement);
}

void instruction_emitter::load_r32(
  cpu_registers destination,
  cpu_registers base,
  std::int32_t displacement)
{
    emit_memory_operand(
      *this,
      0x8B,
      false,
      destination, base);
    emit_memory_displacement(
      *this,
      displacement);
}

void instruction_emitter::store_r32(
  cpu_registers base,
  std::int32_t displacement,
  cpu_registers source)
{
    emit_memory_operand(
      *this,
      0x89,
      false,
      source,
      base);
    emit_memory_displacement(
      *this,
      displacement);
}

void instruction_emitter::movsxd_r64_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, dst, src);
    emit(0x63);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((dst & 0x07u) << 3u)
        | (src & 0x07u)));
}

void instruction_emitter::add_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, src, dst);
    emit(0x01);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::add_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, src, dst);
    emit(0x01);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::sub_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, src, dst);
    emit(0x29);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::sub_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, src, dst);
    emit(0x29);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::imul_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, dst, src);
    emit(0x0F);
    emit(0xAF);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((dst & 0x07u) << 3u)
        | (src & 0x07u)));
}

void instruction_emitter::imul_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, dst, src);
    emit(0x0F);
    emit(0xAF);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((dst & 0x07u) << 3u)
        | (src & 0x07u)));
}

void instruction_emitter::neg_r32(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, false, 0, dst);
    emit(0xF7);
    emit(
      static_cast<std::uint8_t>(
        0xD8u
        | (dst & 0x07u)));
}

void instruction_emitter::neg_r64(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, true, 0, dst);
    emit(0xF7);
    emit(
      static_cast<std::uint8_t>(
        0xD8u
        | (dst & 0x07u)));
}

void instruction_emitter::and_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, src, dst);
    emit(0x21);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::and_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, src, dst);
    emit(0x21);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::or_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, src, dst);
    emit(0x09);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::or_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, src, dst);
    emit(0x09);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::xor_r32(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, false, src, dst);
    emit(0x31);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::xor_r64(
  cpu_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit_rex(*this, true, src, dst);
    emit(0x31);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::test_r32(
  cpu_registers lhs,
  cpu_registers rhs)
{
    const auto left = register_id(lhs);
    const auto right = register_id(rhs);

    emit_rex(*this, false, right, left);
    emit(0x85);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((right & 0x07u) << 3u)
        | (left & 0x07u)));
}

void instruction_emitter::shl_r32_cl(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, false, 0, dst);
    emit(0xD3);
    emit(
      static_cast<std::uint8_t>(
        0xE0u
        | (dst & 0x07u)));
}

void instruction_emitter::shl_r64_cl(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, true, 0, dst);
    emit(0xD3);
    emit(
      static_cast<std::uint8_t>(
        0xE0u
        | (dst & 0x07u)));
}

void instruction_emitter::sar_r32_cl(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, false, 0, dst);
    emit(0xD3);
    emit(
      static_cast<std::uint8_t>(
        0xF8u
        | (dst & 0x07u)));
}

void instruction_emitter::sar_r64_cl(
  cpu_registers destination)
{
    const auto dst = register_id(destination);

    emit_rex(*this, true, 0, dst);
    emit(0xD3);
    emit(
      static_cast<std::uint8_t>(
        0xF8u
        | (dst & 0x07u)));
}

void instruction_emitter::movd_xmm_r32(
  simd_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit(0x66);
    emit_rex(*this, false, dst, src);
    emit(0x0F);
    emit(0x6E);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((dst & 0x07u) << 3u)
        | (src & 0x07u)));
}

void instruction_emitter::movq_xmm_r64(
  simd_registers destination,
  cpu_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit(0x66);
    emit_rex(*this, true, dst, src);
    emit(0x0F);
    emit(0x6E);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((dst & 0x07u) << 3u)
        | (src & 0x07u)));
}

void instruction_emitter::movd_r32_xmm(
  cpu_registers destination,
  simd_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit(0x66);
    emit_rex(*this, false, src, dst);
    emit(0x0F);
    emit(0x7E);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::movq_r64_xmm(
  cpu_registers destination,
  simd_registers source)
{
    const auto dst = register_id(destination);
    const auto src = register_id(source);

    emit(0x66);
    emit_rex(*this, true, src, dst);
    emit(0x0F);
    emit(0x7E);
    emit(
      static_cast<std::uint8_t>(
        0xC0u
        | ((src & 0x07u) << 3u)
        | (dst & 0x07u)));
}

void instruction_emitter::addss(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF3,
      0x58,
      destination,
      source);
}

void instruction_emitter::subss(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF3,
      0x5C,
      destination,
      source);
}

void instruction_emitter::mulss(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF3,
      0x59,
      destination,
      source);
}

void instruction_emitter::divss(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF3,
      0x5E,
      destination,
      source);
}

void instruction_emitter::addsd(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF2,
      0x58,
      destination,
      source);
}

void instruction_emitter::subsd(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF2,
      0x5C,
      destination,
      source);
}

void instruction_emitter::mulsd(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF2,
      0x59,
      destination,
      source);
}

void instruction_emitter::divsd(
  simd_registers destination,
  simd_registers source)
{
    emit_scalar_sse(
      *this,
      0xF2,
      0x5E,
      destination,
      source);
}

void instruction_emitter::push(
  cpu_registers source)
{
    const auto reg = register_id(source);

    emit_rex(*this, false, 0, reg);
    emit(
      static_cast<std::uint8_t>(
        0x50u + (reg & 0x07u)));
}

void instruction_emitter::pop(
  cpu_registers destination)
{
    const auto reg = register_id(destination);

    emit_rex(*this, false, 0, reg);
    emit(
      static_cast<std::uint8_t>(
        0x58u + (reg & 0x07u)));
}

void instruction_emitter::ret()
{
    emit(0xC3);
}

void instruction_emitter::call_reg(
  cpu_registers target)
{
    const auto reg = register_id(target);

    emit_rex(*this, false, 0, reg);
    emit(0xFF);
    emit(
      static_cast<std::uint8_t>(
        0xD0u
        | (reg & 0x07u)));
}

void instruction_emitter::jmp_rel32()
{
    emit(0xE9);
    emit_u32(*this, 0);
}

void instruction_emitter::jcc_rel32(
  condition_code condition)
{
    const auto cc = std::to_underlying(condition);
    if(cc != std::to_underlying(condition_code::equal)
       && cc != std::to_underlying(condition_code::not_equal)
       && cc != std::to_underlying(condition_code::signed_less))
    {
        throw jit_error{
          "Unsupported x86_64 condition code."};
    }

    emit(0x0F);
    emit(static_cast<std::uint8_t>(0x80u + cc));
    emit_u32(*this, 0);
}

void instruction_emitter::patch_jmp_rel32(
  std::size_t instruction_index,
  std::size_t target_index)
{
    if(instruction_index >= code.size()
       || code[instruction_index] != 0xE9
       || instruction_index + 5 > code.size())
    {
        throw jit_error{
          "Instruction is not an x86_64 JMP rel32."};
    }

    const auto displacement = branch_displacement(
      instruction_index,
      target_index,
      5);
    const auto bits = std::bit_cast<std::uint32_t>(displacement);

    for(std::size_t i = 0; i < sizeof(bits); ++i)
    {
        code[instruction_index + 1 + i] = static_cast<std::uint8_t>(bits >> (i * 8));
    }
}

void instruction_emitter::patch_jcc_rel32(
  std::size_t instruction_index,
  std::size_t target_index)
{
    if(instruction_index + 1 >= code.size()
       || code[instruction_index] != 0x0F
       || (code[instruction_index + 1] != 0x84
           && code[instruction_index + 1] != 0x85
           && code[instruction_index + 1] != 0x8C)
       || instruction_index + 6 > code.size())
    {
        throw jit_error{
          "Instruction is not an x86_64 JCC rel32."};
    }

    const auto displacement = branch_displacement(
      instruction_index,
      target_index,
      6);
    const auto bits = std::bit_cast<std::uint32_t>(displacement);

    for(std::size_t i = 0; i < sizeof(bits); ++i)
    {
        code[instruction_index + 2 + i] = static_cast<std::uint8_t>(bits >> (i * 8));
    }
}

}    // namespace slang::jit::x86_64

// NOLINTEND(readability-magic-numbers)
