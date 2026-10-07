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

namespace aarch64
{
class executable_memory;
class jit_compiler;
};    // namespace aarch64

using executable_memory = aarch64::executable_memory;
using jit_compiler = aarch64::jit_compiler;

#endif

}    // namespace slang::jit