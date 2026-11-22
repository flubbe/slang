/**
 * slang - a simple scripting language.
 *
 * file manager.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <system_error>

#include "filemanager.h"
#include "package.h"
#include "utils.h"

namespace slang
{

bool file_manager::exists(
  const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::exists(p);
    }

    return std::ranges::any_of(
      search_paths,
      [&p](const auto& sp) -> bool
      {
          return fs::exists(sp / p);
      });
}

bool file_manager::is_file(
  const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::is_regular_file(p);
    }

    return std::ranges::any_of(
      search_paths,
      [&p](const auto& sp) -> bool
      {
          return fs::is_regular_file(sp / p);
      });
}

bool file_manager::is_directory(
  const fs::path& p) const
{
    if(p.is_absolute())
    {
        return fs::is_directory(p);
    }

    return std::ranges::any_of(
      search_paths,
      [&p](const auto& sp) -> bool
      {
          return fs::is_directory(sp / p);
      });
}

fs::path file_manager::resolve(
  const fs::path& path) const
{
    if(path.is_absolute())
    {
        if(!fs::is_regular_file(path))
        {
            throw file_error{
              std::format(
                "Resolved path '{}' is not a file.",
                path.string())};
        }
        return fs::canonical(path);
    }

    for(const auto& sp: search_paths)
    {
        if(fs::is_regular_file(sp / path))
        {
            return fs::canonical(sp / path);
        }
    }

    throw file_error{
      std::format(
        "Unable to resolve path '{}'.",
        path.string())};
}

std::vector<std::string> file_manager::get_available_modules() const
{
    std::vector<std::string> modules;
    const std::string ext = "." + package::module_ext;

    for(const auto& sp: search_paths)
    {
        std::error_code ec;
        if(!fs::exists(sp, ec) || !fs::is_directory(sp, ec))
        {
            continue;
        }

        for(const auto& entry: fs::recursive_directory_iterator(sp, fs::directory_options::skip_permission_denied, ec))
        {
            if(ec)
            {
                break;
            }
            if(entry.is_regular_file(ec) && entry.path().extension() == ext)
            {
                auto rel = fs::relative(entry.path(), sp, ec);
                if(!ec)
                {
                    rel.replace_extension("");
                    std::string mod_name = rel.string();
                    slang::utils::replace_all(mod_name, "/", package::delimiter);
                    if(std::ranges::find(modules, mod_name) == modules.end())
                    {
                        modules.push_back(std::move(mod_name));
                    }
                }
            }
        }
    }

    return modules;
}

std::unique_ptr<file_archive> file_manager::open(
  const fs::path& path,
  open_mode mode) const
{
    fs::path resolved_path;

    if(path.is_absolute())
    {
        resolved_path = path;
    }
    else
    {
        for(const auto& sp: search_paths)
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
        throw file_error{
          std::format(
            "Unable to find file '{}' in search paths.",
            path.string())};
    }

    if(mode == open_mode::read)
    {
        return std::make_unique<file_read_archive>(
          resolved_path,
          std::endian::little);
    }

    if(mode == open_mode::write)
    {
        return std::make_unique<file_write_archive>(
          resolved_path,
          std::endian::little);
    }

    throw std::runtime_error{"Invalid file open mode."};
}

}    // namespace slang
