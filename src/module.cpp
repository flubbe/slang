/**
 * slang - a simple scripting language.
 *
 * compiled binary file (=module) support.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <exception>

#include <fmt/core.h>

#include "module.h"

namespace slang
{

/*
 * language_module implementation.
 */

void language_module::add_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::size_t entry_point)
{
    // TODO
    throw std::runtime_error("language_module::add_function not implemented.");
}

void language_module::add_native_function(std::string name, std::string return_type, std::vector<std::string> arg_types, std::string lib_name)
{
    if(std::find_if(header.exports.begin(), header.exports.end(),
                    [&name](const exported_symbol& t) -> bool
                    {
                        return t.name == name;
                    })
       != header.exports.end())
    {
        throw module_error(fmt::format("Cannot add native function: Symbol '{}' already defined.", name));
    }

    function_descriptor desc{{std::move(return_type), std::move(arg_types)}, true, native_function_details{std::move(lib_name)}};
    header.exports.emplace_back(symbol_type::function, name, std::move(desc));
}

}    // namespace slang
