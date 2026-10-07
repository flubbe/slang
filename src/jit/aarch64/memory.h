/**
 * slang - a simple scripting language.
 *
 * Just In Time compiler for AArch64.
 * Executable memory.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <utility>

#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>

namespace slang::jit::aarch64
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

/** Executable memory holding the generated machine code. */
class executable_memory
{
public:
    executable_memory() = default;

    executable_memory(
      const std::vector<std::uint32_t>& machine_code)
    {
        size_ = byte_size(machine_code);
        data_ = mmap(
          nullptr,
          size_,
          PROT_READ | PROT_WRITE,
          MAP_ANON | MAP_PRIVATE | MAP_JIT,
          -1,
          0);

        if(data_ == MAP_FAILED)
        {
            throw std::runtime_error{
              "jit_compiler_aarch64: mmap failed"};
        }

        pthread_jit_write_protect_np(0);

        std::memcpy(
          data_,
          machine_code.data(),
          size_);

        pthread_jit_write_protect_np(1);

        if(mprotect(data_, size_, PROT_READ | PROT_EXEC) != 0)
        {
            munmap(data_, size_);
            throw std::runtime_error{
              "jit_compiler_aarch64: mprotect failed"};
        }

        sys_icache_invalidate(
          data_,
          size_);
    }

    ~executable_memory()
    {
        reset();
    }

    executable_memory(const executable_memory&) = delete;
    executable_memory& operator=(const executable_memory&) = delete;

    executable_memory(executable_memory&& other) noexcept
    : data_{std::exchange(other.data_, nullptr)}
    , size_{std::exchange(other.size_, 0)}
    {
    }

    executable_memory& operator=(executable_memory&& other) noexcept
    {
        if(this != &other)
        {
            reset();

            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }

        return *this;
    }

    void* data() const noexcept
    {
        return data_;
    }

    std::size_t size() const noexcept
    {
        return size_;
    }

    explicit operator bool() const noexcept
    {
        return data_ != nullptr;
    }

    void reset() noexcept
    {
        if(data_ != nullptr)
        {
            munmap(data_, size_);
            data_ = nullptr;
            size_ = 0;
        }
    }

private:
    void* data_{nullptr};
    std::size_t size_{0};
};

}    // namespace slang::jit::aarch64
