/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>

#include "jit/aarch64.h"

namespace si = slang::interpreter;

namespace
{

/** X register size, as `std::int32_t` to avoid some type conversions. */
constexpr auto x_register_size = static_cast<std::int32_t>(sizeof(std::uint64_t));

/**
 * Return the byte size of the data held by a vector.
 *
 * @param v The vector.
 * @returns Returns the byte size of the vector's data.
 */
template<
  typename T,
  typename Allocator>
constexpr std::size_t byte_size(
  const std::vector<T, Allocator>& v) noexcept
{
    return v.size() * sizeof(T);
}

/** Struct helper holding offsets of the stack frame elements. */
struct stack_frame_offsets
{
    /** Offset of the locals. */
    std::size_t locals;

    /** Offset of the stack. */
    std::size_t stack;
};

/** Calculate the stack frame offsets. */
stack_frame_offsets calculate_stack_frame_offsets()
{
    si::stack_frame dummy_frame{{}, 0, 0};

    const auto base =
      reinterpret_cast<std::uintptr_t>(&dummy_frame);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t locals_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.locals) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::size_t stack_offset =
      reinterpret_cast<std::uintptr_t>(&dummy_frame.stack) - base;    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return {
      .locals = locals_offset,
      .stack = stack_offset,
    };
}

}    // namespace

namespace slang::jit
{

jit_function jit_compiler_aarch64::allocate_executable_memory(
  const std::vector<std::uint32_t>& machine_code)
{
    const auto byte_size = ::byte_size(machine_code);

    void* code_ptr = mmap(
      nullptr,
      byte_size,
      PROT_READ | PROT_WRITE,
      MAP_ANON | MAP_PRIVATE | MAP_JIT,
      -1,
      0);

    if(code_ptr == MAP_FAILED)
    {
        throw std::runtime_error{
          "jit_compiler_aarch64: mmap failed"};
    }

    pthread_jit_write_protect_np(0);

    std::memcpy(code_ptr, machine_code.data(), byte_size);

    pthread_jit_write_protect_np(1);

    if(mprotect(code_ptr, byte_size, PROT_READ | PROT_EXEC) != 0)
    {
        munmap(code_ptr, byte_size);
        throw std::runtime_error{
          "jit_compiler_aarch64: mprotect failed"};
    }

    sys_icache_invalidate(code_ptr, byte_size);

    executable_memory_aarch64 memory{code_ptr, byte_size};

    auto function =
      reinterpret_cast<jit_function_pointer>(code_ptr);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    return jit_function{
      std::move(memory),
      function};
}

jit_function jit_compiler_aarch64::compile(
  const std::vector<std::byte>& bytecode)
{
    /*
     * Register Mapping
     * ----------------
     * X0  = Pointer to stack_frame passed at runtime
     * X19 = Direct pointer to frame->locals.data()
     * X20 = Direct pointer to frame->stack.data()
     */

    emitter_aarch64 e;

    /*
     * Prologue.
     */

    e.push_fp_lr();

    // Save callee-saved registers X19, X20 to stack
    e.stp_x_pre(
      register_aarch64::X19,
      register_aarch64::X20,
      register_aarch64::SP,
      -2 * x_register_size);

    // Get dynamic offsets regardless of non-standard layout rules
    const auto [locals_offset, stack_offset] = calculate_stack_frame_offsets();

    // The offsets should always be smaller, otherwise something is wrong.
    assert(locals_offset < std::numeric_limits<std::uint32_t>::max());
    assert(stack_offset < std::numeric_limits<std::uint32_t>::max());

    // Load frame->locals.data() into X19
    e.ldr_x(
      register_aarch64::X19,
      register_aarch64::X0,
      static_cast<std::uint32_t>(locals_offset));

    // Load frame->stack.data() into X20
    e.ldr_x(
      register_aarch64::X20,
      register_aarch64::X0,
      static_cast<std::uint32_t>(stack_offset));

    /*
     * Compile bytecode.
     */

    std::size_t pc = 0;
    std::int32_t stack_depth = 0;    // Simulated stack offset (in bytes)

    while(pc < bytecode.size())
    {
        auto op = static_cast<opcode>(bytecode.at(pc++));

        switch(op)
        {
        case opcode::iconst:
        {
            std::int32_t val{0};
            std::memcpy(&val, &bytecode.at(pc), sizeof(val));
            pc += sizeof(val);

            auto uval = static_cast<std::uint32_t>(val);
            auto low16 = static_cast<std::uint16_t>(uval & 0xFFFFu);              // NOLINT(readability-magic-numbers)
            auto high16 = static_cast<std::uint16_t>((uval >> 16u) & 0xFFFFu);    // NOLINT(readability-magic-numbers)

            // Always emit MOVZ (low 16 bits) + MOVK (high 16 bits if non-zero)
            e.movz(register_aarch64::X0, low16, 0);
            if(high16 != 0)
            {
                e.movk(register_aarch64::X0, high16, 16);    // Shift 16 bits left // NOLINT(readability-magic-numbers)
            }

            e.str_w(register_aarch64::X0, register_aarch64::X20, stack_depth);
            stack_depth += 4;
            break;
        }
        case opcode::iload:
        {
            int64_t local_idx{0};
            std::memcpy(&local_idx, &bytecode.at(pc), sizeof(local_idx));
            pc += sizeof(local_idx);

            // Read from X19 (locals), push to X20 (stack)
            e.ldr_w(register_aarch64::X0, register_aarch64::X19, static_cast<std::uint32_t>(local_idx));
            e.str_w(register_aarch64::X0, register_aarch64::X20, stack_depth);
            stack_depth += 4;
            break;
        }
        case opcode::istore:
        {
            int64_t local_idx{0};
            std::memcpy(&local_idx, &bytecode.at(pc), sizeof(local_idx));
            pc += sizeof(local_idx);

            stack_depth -= 4;
            // Pop from X20 (stack), write to X19 (locals)
            e.ldr_w(register_aarch64::X0, register_aarch64::X20, stack_depth);
            e.str_w(register_aarch64::X0, register_aarch64::X19, static_cast<std::uint32_t>(local_idx));
            break;
        }
        case opcode::iadd:
        {
            e.ldr_w(register_aarch64::X1, register_aarch64::X20, stack_depth - 4);    // RHS
            e.ldr_w(register_aarch64::X0, register_aarch64::X20, stack_depth - 8);    // LHS // NOLINT(readability-magic-numbers)
            e.add_w(register_aarch64::X0, register_aarch64::X0, register_aarch64::X1);
            e.str_w(register_aarch64::X0, register_aarch64::X20, stack_depth - 8);    // NOLINT(readability-magic-numbers)
            stack_depth -= 4;
            break;
        }
        case opcode::isub:
        {
            e.ldr_w(register_aarch64::X1, register_aarch64::X20, stack_depth - 4);    // RHS
            e.ldr_w(register_aarch64::X0, register_aarch64::X20, stack_depth - 8);    // LHS // NOLINT(readability-magic-numbers)
            e.sub_w(register_aarch64::X0, register_aarch64::X0, register_aarch64::X1);
            e.str_w(register_aarch64::X0, register_aarch64::X20, stack_depth - 8);    // NOLINT(readability-magic-numbers)
            stack_depth -= 4;
            break;
        }
        case opcode::ret:
        {
            // Epilogue: restore callee-saved registers

            // Pop X19 and X20
            e.ldp_x_post(register_aarch64::X19, register_aarch64::X20, register_aarch64::SP, 16);    // NOLINT(readability-magic-numbers)

            e.pop_fp_lr();
            e.ret();
            break;
        }
        default:
            throw std::runtime_error{"Unimplemented opcode."};
        }
    }

    return allocate_executable_memory(e.code);
}

}    // namespace slang::jit
