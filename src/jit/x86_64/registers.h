/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for x86_64.
 * Register definitions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstdint>

namespace slang::jit::x86_64
{

/** x86_64 general-purpose register encodings. */
enum class cpu_registers : std::uint8_t
{
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

/** x86_64 SIMD registers used by SSE/SSE2 scalar instructions. */
enum class simd_registers : std::uint8_t
{
    XMM0 = 0,
    XMM1 = 1,
    XMM2 = 2,
    XMM3 = 3,
    XMM4 = 4,
    XMM5 = 5,
    XMM6 = 6,
    XMM7 = 7,
    XMM8 = 8,
    XMM9 = 9,
    XMM10 = 10,
    XMM11 = 11,
    XMM12 = 12,
    XMM13 = 13,
    XMM14 = 14,
    XMM15 = 15
};

/** Supported x86_64 condition codes for near conditional branches. */
enum class condition_code : std::uint8_t
{
    equal = 0x4,
    not_equal = 0x5,
    signed_less = 0xC
};

}    // namespace slang::jit::x86_64
