/**
 * slang - a simple scripting language.
 *
 * compiled binary file (=module) support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>

#include <fmt/core.h>

#include "module.h"

namespace slang
{

/*
 * language_module implementation.
 */

std::size_t language_module::add_import(symbol_type type, std::string name, std::uint32_t package_index)
{
    if(std::find_if(header.imports.begin(), header.imports.end(),
                    [&name](const imported_symbol& s) -> bool
                    {
                        return s.name == name;
                    })
       != header.imports.end())
    {
        throw module_error(fmt::format("Cannot add import: Symbol '{}' already defined.", name));
    }

    header.imports.emplace_back(type, std::move(name), package_index);
    return header.imports.size() - 1;
}

void language_module::add_function(std::string name,
                                   std::pair<std::string, bool> return_type,
                                   std::vector<std::pair<std::string, bool>> arg_types,
                                   std::size_t size, std::size_t entry_point, std::vector<variable> locals)
{
    if(std::find_if(header.exports.begin(), header.exports.end(),
                    [&name](const exported_symbol& s) -> bool
                    {
                        return s.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add function: Symbol '{}' already defined.", name));
    }

    function_descriptor desc{{std::move(return_type), std::move(arg_types)}, false, function_details{size, entry_point, locals}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

void language_module::add_native_function(std::string name,
                                          std::pair<std::string, bool> return_type,
                                          std::vector<std::pair<std::string, bool>> arg_types,
                                          std::string lib_name)
{
    if(std::find_if(header.exports.begin(), header.exports.end(),
                    [&name](const exported_symbol& s) -> bool
                    {
                        return s.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add native function: Symbol '{}' already defined.", name));
    }

    function_descriptor desc{{std::move(return_type), std::move(arg_types)}, true, native_function_details{std::move(lib_name)}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

void language_module::add_type(std::string name, std::vector<std::pair<std::string, type_info>> members)
{
    if(std::find_if(header.exports.begin(), header.exports.end(),
                    [&name](const exported_symbol& s) -> bool
                    {
                        return s.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add type: Symbol '{}' already defined.", name));
    }

    type_descriptor desc{std::move(members)};
    header.exports.emplace_back(symbol_type::type, name, std::move(desc));
}

}    // namespace slang
