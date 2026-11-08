/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for POSIX systems.
 * Executable memory.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef __APPLE__
#    include <libkern/OSCacheControl.h>
#    include <pthread.h>
#endif

#include <sys/mman.h>

namespace slang::jit::posix
{

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

/**
 * Flush the processor instruction cache after writing machine code into executable memory.
 *
 * This ensures the CPU sees the newly generated instructions before execution begins.
 * The implementation varies by platform: Apple uses `sys_icache_invalidate()`, while
 * x86-64 uses `__builtin___clear_cache()`.
 *
 * @param addr Pointer to the first byte of the code region that was modified.
 * @param len  Number of bytes in the modified code region.
 */
inline void flush_instruction_cache(
  void* addr,
  size_t len)
{
    // NOLINTNEXTLINE(readability-use-concise-preprocessor-directives)
#if defined(__APPLE__)
    sys_icache_invalidate(addr, len);
#elif defined(__x86_64__) || defined(_M_X64)
    __builtin___clear_cache((char*)addr, (char*)addr + len);
#else
#    error "Unsupported platform for instruction cache invalidation"
#endif
}

/** Executable memory holding the generated machine code. */
class executable_memory
{
    /** Data storage, pointing to executable memory. */
    void* memory{nullptr};

    /** Data storage size. */
    std::size_t memory_size{0};

public:
    /** Default constructor. */
    executable_memory() = default;

    /**
     * Creates executable memory containing the given machine code.
     *
     * @param machine_code Machine code to copy into executable memory.
     */
    explicit executable_memory(
      std::span<const std::byte> machine_code)
    : memory_size{machine_code.size()}
    {
        if(machine_code.empty())
        {
            throw std::invalid_argument{"Cannot create executable memory for empty machine code."};
        }

        int flags = MAP_ANON | MAP_PRIVATE;
#ifdef __APPLE__
        flags |= MAP_JIT;
#endif
        memory = mmap(
          nullptr,
          memory_size,
          PROT_READ | PROT_WRITE,
          flags,
          -1,
          0);

        if(memory == MAP_FAILED)
        {
            memory = nullptr;
            throw std::runtime_error{
              "JIT executable memory allocation failed."};
        }

#ifdef __APPLE__
        pthread_jit_write_protect_np(0);
#endif

        std::memcpy(
          memory,
          machine_code.data(),
          memory_size);

#ifdef __APPLE__
        pthread_jit_write_protect_np(1);
#endif

        if(mprotect(memory, memory_size, PROT_READ | PROT_EXEC) != 0)
        {
            munmap(memory, memory_size);
            memory = nullptr;
            memory_size = 0;
            throw std::runtime_error{
              "JIT executable memory protection failed."};
        }

        flush_instruction_cache(
          memory,
          memory_size);
    }

    /**
     * Creates executable memory containing the given machine code.
     *
     * @param machine_code Machine code to copy into executable memory, represented
     *     as 8-bit values.
     */
    explicit executable_memory(
      std::span<const std::uint8_t> machine_code)
    : executable_memory{std::as_bytes(machine_code)}
    {
    }

    /**
     * Creates executable memory containing the given machine code.
     *
     * @param machine_code Machine code to copy into executable memory, represented
     *     as 32-bit words.
     */
    explicit executable_memory(
      std::span<const std::uint32_t> machine_code)
    : executable_memory{std::as_bytes(machine_code)}
    {
    }

    /** Destructor. */
    ~executable_memory()
    {
        reset();
    }

    /** Disallow copies. */
    executable_memory(const executable_memory&) = delete;

    /** Disallow copy asignments. */
    executable_memory& operator=(const executable_memory&) = delete;

    /** Move constructor. */
    executable_memory(
      executable_memory&& other) noexcept
    : memory{std::exchange(other.memory, nullptr)}
    , memory_size{std::exchange(other.memory_size, 0)}
    {
    }

    /** Move assignment. */
    executable_memory& operator=(executable_memory&& other) noexcept
    {
        if(this != &other)
        {
            reset();

            memory = std::exchange(other.memory, nullptr);
            memory_size = std::exchange(other.memory_size, 0);
        }

        return *this;
    }

    /** Get the executable memory. */
    [[nodiscard]]
    void* data() const noexcept
    {
        return memory;
    }

    /** Get the size of the executable memory. */
    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return memory_size;
    }

    /** Check whether the memory is valid. */
    explicit operator bool() const noexcept
    {
        return memory != nullptr;
    }

    /** Release the allocated resources and reset the memory. */
    void reset() noexcept
    {
        if(memory != nullptr)
        {
            munmap(memory, memory_size);
            memory = nullptr;
            memory_size = 0;
        }
    }
};

}    // namespace slang::jit::posix
