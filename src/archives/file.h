/**
 * slang - a simple scripting language.
 *
 * file read/write support for archives.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <fstream>
#include <filesystem>

#include "archive.h"

namespace fs = std::filesystem;

namespace slang
{

/** Base class for file readers and writers. */
class file_archive : public archive
{
protected:
    /** The file path. */
    fs::path path;

    /** The file stream. */
    std::fstream file;

public:
    /** Defaulted and deleted constructors. */
    file_archive() = delete;
    file_archive(const file_archive&) = delete;
    file_archive(file_archive&&) = default;

    /** Default assignments. */
    file_archive& operator=(const file_archive&) = delete;
    file_archive& operator=(file_archive&&) = default;

    /**
     * Construct a `file_archive` from a path.
     *
     * @param path The file path.
     * @param read Whether the file is readable.
     * @param write Whether the file is writable.
     * @param target_byte_order The target byte order for persistent archives.
     */
    file_archive(fs::path path, bool read, bool write, endian target_byte_order = endian::little);
};

/** A file writer. */
class file_write_archive : public file_archive
{
protected:
    void serialize_bytes(std::byte* buffer, std::size_t c) override
    {
        file.write(reinterpret_cast<const char*>(buffer), c);
    }

public:
    /** Defaulted and deleted constructors. */
    file_write_archive() = delete;
    file_write_archive(const file_write_archive&) = delete;
    file_write_archive(file_write_archive&&) = default;

    /** Assignments. */
    file_write_archive& operator=(const file_write_archive&) = delete;
    file_write_archive& operator=(file_write_archive&&) = default;

    /**
     * Open a file for writing.
     *
     * @param path The file path.
     * @param byte_order The archive's byte order. Defaults to endian::little.
     */
    file_write_archive(fs::path path, endian byte_order = endian::little)
    : file_archive{std::move(path), false, true, byte_order}
    {
    }

    std::size_t tell() override
    {
        return static_cast<std::size_t>(file.tellp());
    }

    std::size_t seek(std::size_t pos) override
    {
        file.seekp(pos, std::ios::beg);
        return tell();
    }

    std::size_t size() override
    {
        // prefer fstream calls over filesystem calls to avoid running into buffer race conditions.
        auto cur = tell();
        auto beg = seek(0);

        file.seekp(0, std::ios::end);
        auto end = tell();

        seek(cur);

        return end - beg;
    }
};

/** A file reader. */
class file_read_archive : public file_archive
{
protected:
    void serialize_bytes(std::byte* buffer, std::size_t c) override
    {
        file.read(reinterpret_cast<char*>(buffer), c);
    }

public:
    /** Defaulted and deleted constructors. */
    file_read_archive() = delete;
    file_read_archive(const file_read_archive&) = delete;
    file_read_archive(file_read_archive&&) = default;

    /** Assignments. */
    file_read_archive& operator=(const file_read_archive&) = delete;
    file_read_archive& operator=(file_read_archive&&) = default;

    /**
     * Open a file for reading.
     *
     * @param path The file path.
     * @param byte_order The archive's byte order. Defaults to endian::little.
     */
    file_read_archive(fs::path path, endian byte_order = endian::little)
    : file_archive{std::move(path), true, false, byte_order}
    {
    }

    std::size_t tell() override
    {
        return static_cast<std::size_t>(file.tellg());
    }

    std::size_t seek(std::size_t pos) override
    {
        file.seekg(pos, std::ios::beg);
        return tell();
    }

    std::size_t size() override
    {
        // prefer fstream calls over filesystem calls to avoid running into buffer race conditions.
        auto cur = tell();
        auto beg = seek(0);

        file.seekg(0, std::ios::end);
        auto end = tell();

        seek(cur);

        return end - beg;
    }
};

}    // namespace slang
