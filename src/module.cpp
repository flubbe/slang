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

namespace slang::module_
{

/*
 * type encoding and decoding.
 */

/** Type encoding pairs as `(type, encoded_type)`. */
static std::vector<std::pair<std::string, std::string>> type_encoding = {
  {"void", "v"},
  {"i32", "i"},
  {"f32", "f"},
  {"str", "s"},
  {"@addr", "a"}};

static constexpr char type_prefix = 'C';

std::string encode_type(const std::string& t)
{
    auto it = std::find_if(type_encoding.begin(), type_encoding.end(),
                           [&t](const std::pair<std::string, std::string>& p) -> bool
                           { return p.first == t; });

    if(it != type_encoding.end())
    {
        return it->second;
    }

    // assume it is a struct.
    return fmt::format("{}{};", type_prefix, t);
}

std::string decode_type(const std::string& t)
{
    auto it = std::find_if(type_encoding.begin(), type_encoding.end(),
                           [&t](const std::pair<std::string, std::string>& p) -> bool
                           { return p.second == t; });

    if(it != type_encoding.end())
    {
        return it->first;
    }
    else if(t.length() >= 3 && t[0] == type_prefix)
    {
        if(t[t.length() - 1] != ';')
        {
            throw module_error("Cannot decode type with invalid name.");
        }
        return t.substr(1, t.length() - 2);
    }

    throw module_error(fmt::format("Cannot decode unknown type '{}'.", t));
}

archive& operator&(archive& ar, type& ts)
{
    if(ar.is_reading())
    {
        std::uint8_t c;
        std::string s;

        ar & c;
        s += c;
        if(c == type_prefix)
        {
            do
            {
                ar & c;
                s += c;
            } while(c != ';');
        }

        ts.decode(s);
    }
    else if(ar.is_writing())
    {
        auto t = ts.encode();
        std::uint8_t c = t[0];

        ar & c;
        if(c == type_prefix)
        {
            if(t.length() < 3)
            {
                throw module_error("Cannot encode empty struct type name.");
            }

            std::size_t i = 1;
            do
            {
                c = t[i];
                ++i;

                ar & c;
            } while(c != ';' && i < t.length());

            if(c != ';')
            {
                throw module_error("Cannot encode invalid struct type name.");
            }
        }
    }
    else
    {
        throw module_error("Invalid archive mode.");
    }

    return ar;
}

/*
 * language_module implementation.
 */

std::size_t language_module::add_import(symbol_type type, std::string name, std::uint32_t package_index)
{
    auto it = std::find_if(header.imports.begin(), header.imports.end(),
                           [type, &name](const imported_symbol& s) -> bool
                           {
                               return s.type == type && s.name == name;
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
                        return s.type == symbol_type::function && s.name == name;
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
                        return s.type == symbol_type::function && s.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add native function: '{}' already defined.", name));
    }

    function_descriptor desc{{std::move(return_type), std::move(arg_types)}, true, native_function_details{std::move(lib_name)}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

void language_module::add_type(std::string name, std::vector<std::pair<std::string, type_info>> members)
{
    if(std::find_if(header.exports.begin(), header.exports.end(),
                    [&name](const exported_symbol& s) -> bool
                    {
                        return s.type == symbol_type::type && s.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add type: '{}' already defined.", name));
    }

    type_descriptor desc{std::move(members)};
    header.exports.emplace_back(symbol_type::type, name, std::move(desc));
}

}    // namespace slang::module_
