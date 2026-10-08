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

#if defined(SLANG_ARCH_AARCH64) && defined(SLANG_OS_POSIX)
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
class jit_compiler;
}    // namespace aarch64

using jit_compiler = aarch64::jit_compiler;

#endif /* SLANG_ARCH_AARCH64 */

#ifdef SLANG_OS_POSIX

namespace posix
{
class executable_memory;
}    // namespace posix

using executable_memory = posix::executable_memory;

#endif /* SLANG_OS_POSIX */

}    // namespace slang::jit
