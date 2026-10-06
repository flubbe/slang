#pragma once

#include <utility>

#include <sys/mman.h>

namespace slang::jit
{

class executable_memory_aarch64
{
public:
    executable_memory_aarch64() = default;

    executable_memory_aarch64(void* data, std::size_t size)
    : data_{data}
    , size_{size}
    {
    }

    ~executable_memory_aarch64()
    {
        reset();
    }

    executable_memory_aarch64(const executable_memory_aarch64&) = delete;
    executable_memory_aarch64& operator=(const executable_memory_aarch64&) = delete;

    executable_memory_aarch64(executable_memory_aarch64&& other) noexcept
    : data_{std::exchange(other.data_, nullptr)}
    , size_{std::exchange(other.size_, 0)}
    {
    }

    executable_memory_aarch64& operator=(executable_memory_aarch64&& other) noexcept
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

}    // namespace slang::jit
