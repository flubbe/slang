/**
 * slang - a simple scripting language.
 *
 * Common Just In Time compiler helpers and definitions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <utility>

#include "interpreter/interpreter.h"
#include "platform.h"

/*
 * Platform specific code.
 */

#if defined(SLANG_ARCH_AARCH64)
#    include "aarch64/memory.h"
#else
#    error "JIT executable_memory is not supported on this target architecture."
#endif

namespace slang::jit
{

/*
 * A JIT compiled function.
 */

namespace si = slang::interpreter;

/** Signature of a JIT compiled function. */
using jit_function_pointer = std::uint64_t (*)(si::stack_frame* frame);

/** A Just In Time compiled function. */
class jit_function
{
    /** Memory holding the compiled function. */
    executable_memory memory;

    /** The function pointer. */
    jit_function_pointer function;

public:
    /** Deleted default constructor. */
    jit_function() = delete;

    /** Deleted copy constructor. */
    jit_function(const jit_function&) = delete;

    /** Move constructor. */
    jit_function(jit_function&& other)
    : memory{std::move(other.memory)}
    , function{std::exchange(other.function, nullptr)}
    {
    }

    /**
     * Construct a new JIT compiled function.
     *
     * @param memory Executable memory holding the function's code.
     * @param function Function pointer.
     */
    jit_function(
      executable_memory memory,
      jit_function_pointer function)
    : memory{std::move(memory)}
    , function{function}
    {
    }

    /** Deleted copy assignment. */
    jit_function& operator=(const jit_function&) = delete;

    /** Move assignment. */
    jit_function& operator=(jit_function&& other)
    {
        if(this != &other)
        {
            memory = std::move(other.memory);
            function = std::exchange(other.function, nullptr);
        }

        return *this;
    }

    /** Return the function pointer. */
    [[nodiscard]]
    jit_function_pointer get() const noexcept
    {
        return function;
    }

    /**
     * Invoke the function.
     *
     * @param frame The stack frame passed to the function.
     * @returns TODO
     */
    std::uint64_t operator()(
      si::stack_frame* frame) const
    {

        assert(function != nullptr && "Attempted to invoke a moved-from jit_function!");
        return function(frame);
    }

    /**
     * Check whether the function is valid.
     *
     * @note The only way a function can become invalid is when it was moved from.
     */
    explicit operator bool() const noexcept
    {
        return function != nullptr;
    }
};

}    // namespace slang::jit
