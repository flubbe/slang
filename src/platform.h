/**
 * slang - a simple scripting language.
 *
 * Platform detection.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#if defined(__aarch64__) || defined(_M_ARM64)
#    define SLANG_ARCH_AARCH64 1
#elif defined(__x86_64__) || defined(_M_X64)
#    define SLANG_ARCH_X86_64 1
#endif
