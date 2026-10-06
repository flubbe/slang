/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 * Register definitions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <cstdint>
#include <type_traits>

namespace slang::jit
{

/**
 * @brief AArch64 64-bit General-Purpose Registers (AAPCS64 ABI).
 *
 * Bit-encodings correspond directly to the 5-bit register fields in AArch64 machine instructions.
 */
enum class register_aarch64 : std::uint32_t
{
    /*
     * Parameter Passing & Volatile Scratch Registers (Caller-Saved).
     */
    X0 = 0, /**< Argument 1 / Primary Scalar Return Value / Scratch. */
    X1 = 1, /**< Argument 2 / Secondary Return Value / Scratch. */
    X2 = 2, /**< Argument 3 / Scratch. */
    X3 = 3, /**< Argument 4 / Scratch. */
    X4 = 4, /**< Argument 5 / Scratch. */
    X5 = 5, /**< Argument 6 / Scratch. */
    X6 = 6, /**< Argument 7 / Scratch. */
    X7 = 7, /**< Argument 8 / Scratch. */

    /*
     * Indirect Result & Caller-Saved Scratch Registers.
     */
    X8 = 8,   /**< Indirect result location pointer (e.g., struct return) / Scratch. */
    X9 = 9,   /**< Temporary / Scratch. */
    X10 = 10, /**< Temporary / Scratch. */
    X11 = 11, /**< Temporary / Scratch. */
    X12 = 12, /**< Temporary / Scratch. */
    X13 = 13, /**< Temporary / Scratch. */
    X14 = 14, /**< Temporary / Scratch. */
    X15 = 15, /**< Temporary / Scratch. */

    /*
     * Special Linker & Platform Registers (Caller-Saved).
     */
    X16 = 16, /**< IP0 - Intra-Procedure-call scratch 1 (used by dynamic linker / PLT veneers). */
    X17 = 17, /**< IP1 - Intra-Procedure-call scratch 2 (used by dynamic linker / PLT veneers). */
    X18 = 18, /**< Platform Register / Reserved (macOS/iOS: Thread Local Storage pointer). */

    /*
     * Preserved Registers (Callee-Saved).
     *
     * Must be pushed to stack in prologue if modified and restored in epilogue.
     */
    X19 = 19, /**< Callee-saved register. */
    X20 = 20, /**< Callee-saved register. */
    X21 = 21, /**< Callee-saved register. */
    X22 = 22, /**< Callee-saved register. */
    X23 = 23, /**< Callee-saved register. */
    X24 = 24, /**< Callee-saved register. */
    X25 = 25, /**< Callee-saved register. */
    X26 = 26, /**< Callee-saved register. */
    X27 = 27, /**< Callee-saved register. */
    X28 = 28, /**< Callee-saved register. */

    /*
     * Frame Pointer & Link Register (Callee-Saved).
     */
    X29 = 29, /**< FP - Frame Pointer (points to base of stack frame). */
    FP = 29,  /**< Alias for X29. */

    X30 = 30, /**< LR - Link Register (holds function return address). */
    LR = 30,  /**< Alias for X30. */

    /*
     * Special Purpose Registers (Encoding ID 31).
     *
     * Note: Register ID 31 is context-dependent in AArch64 opcodes.
     */
    SP = 31, /**< Stack Pointer (used when instruction references SP, e.g., ADD/SUB SP, STP/LDP). */
    XZR = 31 /**< Zero Register (reads as 0, writes are discarded; used in general data processing). */
};

template<typename T>
    requires std::is_integral_v<T>
inline std::uint32_t operator<<(
  register_aarch64 r,
  T imm)
{
    return static_cast<std::uint32_t>(r) << imm;
}

template<typename T>
    requires std::is_integral_v<T>
inline T& operator|=(
  T& imm,
  register_aarch64 r)
{
    imm |= static_cast<T>(r);
    return imm;
}

template<typename T>
    requires std::is_integral_v<T>
T operator|(T i, register_aarch64 r)
{
    return i | static_cast<T>(r);
}

}    // namespace slang::jit
