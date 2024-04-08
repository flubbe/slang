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
     * Construct an in-memory archive.
     *
     * @param persistent Whether to mirror the behavior of persistent archive w.r.t. byte ordering.
     * @param byte_order The target byte order. Only relevant if `persistent` is `true`.
     */
    memory_write_archive(bool persistent, endian byte_order = endian::little)
    : archive{false, true, persistent, byte_order}
    {
    }

    std::size_t tell() override
    {
        return memory_buffer.size();
    }

    std::size_t seek(std::size_t pos) override
    {
        throw serialization_error("memory_write_archive::seek: Operation not supported by archive.");
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
