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

/**
 * @brief x86_64 64-bit General-Purpose Registers.
 *
 * Values correspond directly to the 4-bit register encodings in x86_64
 * instructions, including the REX extension bit.
 */
enum class cpu_registers : std::uint8_t
{
    RAX = 0,  /**< Accumulator register. */
    RCX = 1,  /**< Counter register. */
    RDX = 2,  /**< Data register. */
    RBX = 3,  /**< Base register. */
    RSP = 4,  /**< Stack Pointer. */
    RBP = 5,  /**< Base Pointer (commonly used as frame pointer). */
    RSI = 6,  /**< Source Index. */
    RDI = 7,  /**< Destination Index. */
    R8 = 8,   /**< Extended general-purpose register. */
    R9 = 9,   /**< Extended general-purpose register. */
    R10 = 10, /**< Extended general-purpose register. */
    R11 = 11, /**< Extended general-purpose register. */
    R12 = 12, /**< Extended general-purpose register. */
    R13 = 13, /**< Extended general-purpose register. */
    R14 = 14, /**< Extended general-purpose register. */
    R15 = 15, /**< Extended general-purpose register. */
};

/**
 * @brief x86_64 128-bit SIMD registers (SSE/SSE2).
 *
 * Values correspond directly to the 4-bit register encodings in x86_64
 * instructions, including the REX extension bit.
 */
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
    XMM15 = 15,
};

/**
 * @brief x86_64 condition codes for near conditional branches.
 *
 * Values correspond to the low 4 bits of the condition-code opcode field.
 */
enum class condition_code : std::uint8_t
{
    equal = 0x4,       /**< ZF = 1. */
    not_equal = 0x5,   /**< ZF = 0. */
    signed_less = 0xC, /**< SF != OF. */
};

}    // namespace slang::jit::x86_64
