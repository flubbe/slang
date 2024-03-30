/**
 * slang - a simple scripting language.
 *
 * file manager.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "filemanager.h"

namespace slang
{

bool file_manager::exists(const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::exists(p);
    }

    for(auto& sp: search_paths)
    {
        if(fs::exists(sp / p))
        {
            return true;
        }
    }

    return false;
}

bool file_manager::is_file(const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::is_regular_file(p);
    }

    for(auto& sp: search_paths)
    {
        if(fs::is_regular_file(sp / p))
        {
            return true;
        }
    }

    return false;
}

bool file_manager::is_directory(const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::is_directory(p);
    }

    for(auto& sp: search_paths)
    {
        if(fs::is_directory(sp / p))
        {
            return true;
        }
    }

    return false;
}

fs::path file_manager::resolve(const fs::path& path) const
{
    if(path.is_absolute())
    {
        if(!fs::is_regular_file(path))
        {
            throw file_error(fmt::format("Resolved path '{}' is not a file.", path.c_str()));
        }
        return fs::canonical(path);
    }

    for(auto& sp: search_paths)
    {
        if(fs::is_regular_file(sp / path))
        {
            return fs::canonical(sp / path);
        }
    }

    throw file_error(fmt::format("Unable to resolve path '{}'.", path.c_str()));
}

std::unique_ptr<file_archive> file_manager::open(const fs::path& path, open_mode mode) const
{
    fs::path resolved_path;

    if(path.is_absolute())
    {
        resolved_path = path;
    }
    else
    {
        for(auto& sp: search_paths)
        {
            if(fs::is_regular_file(sp / path))
            {
                resolved_path = sp / path;
                break;
            }
        }
    }

    if(resolved_path.empty())
    {
        throw file_error(fmt::format("Unable to find file '{}' in search paths.", resolved_path.c_str()));
    }

    if(mode == open_mode::read)
    {
        return std::make_unique<file_read_archive>(resolved_path, endian::little);
    }
    else if(mode == open_mode::write)
    {
        return std::make_unique<file_write_archive>(resolved_path, endian::little);
    }

    throw std::runtime_error("Invalid file open mode.");
}

}    // namespace slang