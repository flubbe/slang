/**
 * slang - a simple scripting language.
 *
 * Forward declarations used by other components.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include "platform.h"

/*
 * Check JIT availability.
 */

#ifdef SLANG_ARCH_AARCH64
#    define SLANG_JIT_AVAILABLE 1
#else
#    define SLANG_JIT_AVAILABLE 0
#endif

/*
 * Compiler alias.
 */

namespace slang::jit
{

#ifdef SLANG_ARCH_AARCH64

class jit_compiler_aarch64;
using jit_compiler = jit_compiler_aarch64;

class executable_memory_aarch64;
using executable_memory = executable_memory_aarch64;

#endif

}    // namespace slang::jit