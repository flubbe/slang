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
 * type encoding and decoding.
 */

std::string encode_type(const std::string& t)
{
    if(t == "void")
    {
        return "v";
    }
    else if(t == "i32")
    {
        return "i";
    }
    else if(t == "f32")
    {
        return "f";
    }
    else if(t == "str")
    {
        return "s";
    }
    else if(t == "addr")
    {
        return "a";
    }

    // assume it is a struct.
    return fmt::format("S{};", t);
}

std::string decode_type(const std::string& t)
{
    if(t == "v")
    {
        return "void";
    }
    else if(t == "i")
    {
        return "i32";
    }
    else if(t == "f")
    {
        return "f32";
    }
    else if(t == "s")
    {
        return "str";
    }
    else if(t == "a")
    {
        return "addr";
    }
    else if(t.substr(0, 1) == "S")
    {
        if(t.length() < 3)
        {
            throw module_error("Cannot decode empty struct type name.");
        }
        if(t[t.length() - 1] != ';')
        {
            throw module_error("Cannot decode type with invalid name.");
        }
        return t.substr(1, t.length() - 2);
    }

    throw module_error(fmt::format("Cannot decode unknown type '{}'.", t));
}

/*
 * language_module implementation.
 */

std::size_t language_module::add_import(symbol_type type, std::string name, std::uint32_t package_index)
{
    auto it = std::find_if(header.imports.begin(), header.imports.end(),
                           [&name](const imported_symbol& s) -> bool
                           {
                               return s.name == name;
                           });
    if(it != header.imports.end())
    {
        return std::distance(header.imports.begin(), it);
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
