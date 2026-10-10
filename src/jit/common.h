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

#include <cassert>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "interpreter/interpreter.h"
#include "platform.h"

/*
 * Platform specific code.
 */

#ifdef SLANG_OS_POSIX
#    include "memory_posix.h"
#else
#    error "JIT executable_memory is not supported on this target architecture."
#endif

namespace slang::jit
{

namespace si = slang::interpreter;

/** JIT compiler error. */
class jit_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/*
 * A JIT compiled function.
 */

/** Signature of a JIT compiled function. */
using jit_function_pointer = void (*)(si::stack_frame* frame);

/** Signature of a native function. */
using native_function_type = std::function<void(si::operand_stack&)>;

/** A JIT callable function, either native or JIT compiled. */
using jit_callable = std::variant<
  std::monostate,
  jit_function_pointer,
  native_function_type>;

/** A Just In Time compiled function. */
class jit_function
{
    /** Memory holding the compiled function. */
    executable_memory memory;

    /** The function pointer. */
    jit_function_pointer function;

    /** Locals size. */
    std::size_t locals_size = 0;

    /** Stack size. */
    std::size_t stack_size = 0;

public:
    /** Deleted default constructor. */
    jit_function() = delete;

    /** Deleted copy constructor. */
    jit_function(const jit_function&) = delete;

    /** Move constructor. */
    jit_function(jit_function&& other)
    : memory{std::move(other.memory)}
    , function{std::exchange(other.function, nullptr)}
    , locals_size{std::exchange(other.locals_size, 0)}
    , stack_size{std::exchange(other.stack_size, 0)}
    {
    }

    /**
     * Construct a new JIT compiled function.
     *
     * @param memory Executable memory holding the function's code.
     * @param function Function pointer.
     * @param locals_size Bytes needed for the locals.
     * @param stack_size Bytes needed for the stack.
     */
    jit_function(
      executable_memory memory,
      jit_function_pointer function,
      std::size_t locals_size,
      std::size_t stack_size)
    : memory{std::move(memory)}
    , function{function}
    , locals_size{locals_size}
    , stack_size{stack_size}
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
            locals_size = std::exchange(other.locals_size, 0);
            stack_size = std::exchange(other.stack_size, 0);
        }

        return *this;
    }

    /** Return the function pointer. */
    [[nodiscard]]
    jit_function_pointer get() const noexcept
    {
        return function;
    }

    /** Return the locals size, in bytes. */
    [[nodiscard]]
    std::size_t get_locals_size() const noexcept
    {
        return locals_size;
    }

    /** Return the required stack size, in bytes. */
    [[nodiscard]]
    std::size_t get_stack_size() const noexcept
    {
        return stack_size;
    }

    /**
     * Invoke the function.
     *
     * @param frame The stack frame passed to the function.
     */
    void operator()(
      si::stack_frame* frame) const
    {

        assert(function != nullptr && "Attempted to invoke a moved-from jit_function!");
        function(frame);
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

/** Stable target metadata referenced by generated call instructions. */
struct jit_call_target
{
    /** Native or JITted function. */
    jit_callable function{std::monostate{}};

    /** Native library name used to resolve native function registrations. */
    std::optional<std::string> native_library;

    /** Pointer to the module constant table. */
    const std::vector<module_::constant_table_entry>* constants{nullptr};

    /** Argument size, in bytes. */
    std::size_t argument_size{0};

    /** Return value size, in bytes. */
    std::size_t return_size{0};

    /** Locals size, in bytes. */
    std::size_t locals_size{0};

    /** Required stack size, in bytes. */
    std::size_t stack_size{0};
};

}    // namespace slang::jit
