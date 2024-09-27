/**
 * slang - a simple scripting language.
 *
 * file manager.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "archives/file.h"

namespace fs = std::filesystem;

namespace slang
{

/**
 * A file error.
 */
class file_error : public std::runtime_error
{
public:
    /**
     * Construct a file_error.
     *
     * @param message The error message.
     */
    file_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * A file manager, used for path resolution.
 */
class file_manager
{
    /** Search paths, used for resolving non-fully-qualified names. */
    std::vector<fs::path> search_paths;

public:
    /** Default constructors. */
    file_manager() = default;
    file_manager(const file_manager&) = default;
    file_manager(file_manager&&) = default;

    /** Default assignments. */
    file_manager& operator=(const file_manager&) = default;
    file_manager& operator=(file_manager&&) = default;

    /** File opening modes. */
    enum class open_mode
    {
        read,
        write
    };

    /**
     * Add a search path. Does nothing if the path already is in the search path list.
     * Does not check if the path actually exists.
     *
     * @param p The search path to add.
     */
    void add_search_path(fs::path p)
    {
        p = fs::canonical(std::move(p));
        if(std::find(search_paths.begin(), search_paths.end(), p) == search_paths.end())
        {
            search_paths.emplace_back(std::move(p));
        }
    }

    /**
     * Check if a path exists. If the path is not an absolute path, the
     * path is checked within the search paths.
     */
    bool exists(const fs::path& p) const;

    /**
     * Check if a path represents a regular file. If the path is not an absolute path,
     * the path is checked within the search paths.
     */
    bool is_file(const fs::path& p) const;

    /**
     * Check if a path represents a directory. If the path is not an absolute path,
     * the path is checked within the search paths.
     */
    bool is_directory(const fs::path& p) const;

    /**
     * Resolve a file name. If the path is not an absolute path, the path is checked
     * within the search paths.
     *
     * @param path The file path to be resolved.
     * @returns The resolved path.
     * @throws A file_error if the path cannot be resolved.
     */
    fs::path resolve(const fs::path& path) const;

    /**
     * Open a file using an archive. If the file path is not an absolute path,
     * the path is check within the search paths.
     *
     * Files are opened as little endian archives.
     *
     * @param path The file path.
     * @param mode The opening mode (read or write).
     * @returns A readable or writable archive representing the file.
     * @throws A file_error if the path cannot be opened.
     */
    std::unique_ptr<file_archive> open(const fs::path& path, open_mode mode) const;
};

}    // namespace slang