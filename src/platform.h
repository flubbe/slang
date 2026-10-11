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

/*
 * Target architecture detection.
 */

#if defined(__aarch64__) || defined(_M_ARM64)
#    define SLANG_ARCH_AARCH64 1
#elif defined(__x86_64__) || defined(_M_X64)
#    define SLANG_ARCH_X86_64 1
#endif

/*
 * Platform/OS detection.
 */

#if defined(_WIN32) || defined(_WIN64)
#    define SLANG_OS_WINDOWS 1
#elif defined(__APPLE__)
#    define SLANG_OS_MACOS 1
#    define SLANG_OS_POSIX 1
#elif defined(__linux__) || defined(__FreeBSD__)
#    define SLANG_OS_LINUX 1
#    define SLANG_OS_POSIX 1
#endif
