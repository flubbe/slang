/**
 * slang - a simple scripting language.
 *
 * package management.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <print>

#include "package.h"

namespace slang
{

/*
 * package implementation.
 */

package::package(const fs::path& path)
: path{fs::exists(path) ? fs::canonical(path) : fs::absolute(path)}
{
}

bool package::contains_module(const std::string& module_name) const
{
    if(!is_valid_name_component(module_name))
    {
        return false;
    }
    fs::path module_path = path / module_name;
    module_path += "." + module_ext;
    return fs::exists(module_path);
}

bool package::contains_source(const std::string& module_name) const
{
    if(!is_valid_name_component(module_name))
    {
        return false;
    }
    fs::path module_path = path / module_name;
    module_path += "." + source_ext;
    return fs::exists(module_path);
}

bool package::is_persistent() const
{
    return fs::exists(path);
}

void package::make_persistent()
{
    if(!is_persistent())
    {
        fs::create_directories(path);
        path = fs::canonical(path);
    }
}

std::vector<std::string> package::split(const std::string& s)
{
    return slang::utils::split(s, delimiter);
}

bool package::is_valid_name(const std::string& name)
{
    if(name.empty())
    {
        return false;
    }

    // ensure the delimiters only occur inside the name.
    if(name.length() >= delimiter.length())
    {
        if(name.substr(0, delimiter.length()) == delimiter
           || name.substr(name.length() - delimiter.length(), std::string::npos) == delimiter)
        {
            return false;
        }
    }

    // split name at "::" occurences.
    auto components = split(name);

    // validate components
    return std::all_of(components.begin(), components.end(), is_valid_name_component);
}

bool package::is_valid_name_component(const std::string& name)
{
    char ch = name[0];
    if(ch != '_' && std::isalpha(ch) == 0)
    {
        return false;
    }

    return std::all_of(
      std::next(name.begin()),
      name.end(),
      [](char ch)
      { return ch == '_' || std::isalnum(ch) != 0; });
}

/*
 * package_manager implementation.
 */

/**
 * Return the canonical form of the path. Creates the path if it does not exist.
 *
 * @param path The path to create.
 * @throws `std::bad_alloc` on allocation failure and `fs::filesystem_error`
 *         when path creation fails or the canonical path cannot be obtained.
 */
static fs::path get_path(const fs::path& path)
{
    if(!fs::exists(path))
    {
        fs::create_directories(path);
    }
    return fs::canonical(path);
}

package_manager::package_manager(const fs::path& in_package_root, bool create)
{
    if(create)
    {
        package_root = get_path(in_package_root);
    }
    else
    {
        // store the canonical path for existing directories.
        if(fs::exists(in_package_root))
        {
            package_root = fs::canonical(in_package_root);
        }
        else
        {
            // the path will be lazily created when used.
            package_root = fs::absolute(in_package_root);
        }
    }
}

package package_manager::open(const std::string& name, bool create)
{
    if(!package::is_valid_name(name))
    {
        throw std::runtime_error(std::format("package_manager::open: Invalid package name '{}'.", name));
    }

    fs::path package_path = package_root;
    std::vector<std::string> components = package::split(name);
    for(const auto& c: components)
    {
        package_path /= c;
    }

    if(!fs::exists(package_path))
    {
        if(!create)
        {
            throw std::runtime_error(std::format("package_manager::open: Cannot find '{}'.", package_path.string()));
        }

        // create the package.
        package_path = get_path(package_path);
    }
    else
    {
        package_path = fs::canonical(package_path);
    }

    return package{package_path};
}

void package_manager::remove(const std::string& name)
{
    if(!package::is_valid_name(name))
    {
        throw std::runtime_error(std::format("package_manager::remove: Invalid package name '{}'.", name));
    }

    if(!exists(name))
    {
        throw std::runtime_error(std::format("package_manager::remove: Package '{}' does not exist.", name));
    }

    fs::path package_path = package_root;
    std::vector<std::string> components = package::split(name);
    for(const auto& c: components)
    {
        package_path /= c;
    }

    std::print("Remove: {}\n", package_path.string());

    if(fs::remove_all(package_path) == 0)
    {
        throw std::runtime_error(std::format("Could not remove package '{}'.", name));
    }
}

bool package_manager::exists(const std::string& name) const
{
    if(!package::is_valid_name(name))
    {
        throw std::runtime_error(std::format("package_manager::exists: Invalid package name '{}'.", name));
    }

    fs::path package_path = package_root;

    std::vector<std::string> components = package::split(name);
    for(const auto& c: components)
    {
        package_path /= c;
    }

    return fs::exists(package_path);
}

std::vector<std::string> package_manager::get_package_names(bool include_sub_packages, std::optional<std::string> parent)
{
    std::vector<std::vector<std::string>> package_name_components;
    fs::path search_root = package_root;

    if(parent)
    {
        if(!package::is_valid_name(*parent))
        {
            throw std::runtime_error(std::format("The name '{}' is not a valid package name.", *parent));
        }

        if(!exists(*parent))
        {
            throw std::runtime_error(std::format("The parent package '{}' does not exist.", *parent));
        }

        std::vector<std::string> components = slang::utils::split(*parent, package::delimiter);
        for(auto& c: components)
        {
            search_root /= c;
        }
    }

    if(!include_sub_packages)
    {
        for(auto const& dir_entry: fs::directory_iterator{search_root})
        {
            if(dir_entry.is_directory())
            {
                fs::path relative_path = fs::relative(dir_entry.path(), package_root);
                package_name_components.push_back({relative_path.string()});
            }
        }
    }
    else
    {
        for(auto const& dir_entry: fs::recursive_directory_iterator{search_root})
        {
            if(dir_entry.is_directory())
            {
                fs::path relative_path = fs::relative(dir_entry.path(), package_root);
                std::vector<fs::path::iterator::value_type> path_components(relative_path.begin(), relative_path.end());
                std::vector<std::string> string_components;
                string_components.reserve(path_components.size());

                std::for_each(path_components.begin(), path_components.end(), [&string_components](auto& c)
                              { string_components.push_back(c.string()); });
                package_name_components.push_back(string_components);
            }
        }
    }

    std::sort(package_name_components.begin(), package_name_components.end());

    std::vector<std::string> package_names;
    package_names.reserve(package_name_components.size());
    for(auto& c: package_name_components)
    {
        package_names.push_back(slang::utils::join(c, package::delimiter));
    }

    return package_names;
}

bool package_manager::is_persistent() const
{
    return fs::exists(package_root);
}

void package_manager::make_persistent()
{
    if(!is_persistent())
    {
        fs::create_directories(package_root);
        package_root = fs::canonical(package_root);
    }
}

}    // namespace slang
