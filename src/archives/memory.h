/**
 * slang - a simple scripting language.
 *
 * in-memory archives.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <vector>

#include "archive.h"

namespace slang
{

/** Archive for in-memory writes. */
class memory_write_archive : public archive
{
protected:
    /** The archive buffer. */
    std::vector<std::byte> memory_buffer;

    void serialize_bytes(std::byte* buffer, std::size_t c) override
    {
        memory_buffer.insert(memory_buffer.end(), buffer, buffer + c);
    }

public:
    /** Defaulted and deleted constructors. */
    memory_write_archive() = delete;
    memory_write_archive(const memory_write_archive&) = default;
    memory_write_archive(memory_write_archive&&) = default;

    /** Default assignments. */
    memory_write_archive& operator=(const memory_write_archive&) = default;
    memory_write_archive& operator=(memory_write_archive&&) = default;

    /**
     * Construct an in-memory write archive.
     *
     * @param persistent Whether to mirror the behavior of persistent archive w.r.t. byte ordering.
     * @param byte_order The target byte order. Only relevant if `persistent` is `true`.
     */
    memory_write_archive(bool persistent, endian byte_order = endian::native)
    : archive{false, true, persistent, byte_order}
    {
    }

    std::size_t tell() override
    {
        return memory_buffer.size();
    }

    std::size_t seek([[maybe_unused]] std::size_t pos) override
    {
        throw serialization_error("memory_write_archive::seek: Operation not supported by archive.");
    }

    std::size_t size() override
    {
        return memory_buffer.size();
    }

    /** Clear the internal buffer. */
    void clear()
    {
        memory_buffer.clear();
    }

    /** Get the internal buffer. */
    const std::vector<std::byte>& get_buffer() const
    {
        return memory_buffer;
    }
};

/** Archive for in-memory reads. */
class memory_read_archive : public archive
{
protected:
    /** The archive's buffer reference. */
    const std::vector<std::byte>& memory_buffer;

    /** Current buffer read offset. */
    std::size_t offset = 0;

    void serialize_bytes(std::byte* buffer, std::size_t c) override
    {
        if(offset + c > memory_buffer.size())
        {
            throw std::runtime_error("memory_read_archive: read out of bounds.");
        }

        std::copy(memory_buffer.begin() + offset, memory_buffer.begin() + offset + c, buffer);
        offset += c;
    }

public:
    /** Defaulted and deleted constructors. */
    memory_read_archive() = delete;
    memory_read_archive(const memory_read_archive&) = default;
    memory_read_archive(memory_read_archive&&) = default;

    /** Deleted assignments. */
    memory_read_archive& operator=(const memory_read_archive&) = delete;
    memory_read_archive& operator=(memory_read_archive&&) = delete;

    /**
     * Construct an in-memory read archive.
     *
     * @param memory_buffer The underlying buffer.
     * @param persistent Whether to mirror the behavior of persistent archive w.r.t. byte ordering.
     * @param byte_order The target byte order. Only relevant if `persistent` is `true`.
     */
    memory_read_archive(const std::vector<std::byte>& memory_buffer, bool persistent, endian byte_order = endian::native)
    : archive{true, false, persistent, byte_order}
    , memory_buffer{memory_buffer}
    {
    }

    std::size_t tell() override
    {
        return offset;
    }

    std::size_t seek(std::size_t pos) override
    {
        if(pos >= memory_buffer.size())
        {
            throw std::runtime_error("memory_read_archive::seek: position out of bounds.");
        }

        offset = pos;
        return offset;
    }

    std::size_t size() override
    {
        return memory_buffer.size();
    }

    /** Get the internal buffer. */
    const std::vector<std::byte>& get_buffer() const
    {
        return memory_buffer;
    }
};

}    // namespace slang
