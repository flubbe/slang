/**
 * slang - a simple scripting language.
 *
 * type system context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "typing.h"

namespace slang::typing
{

/*
 * Exceptions.
 */

type_error::type_error(const token_location& loc, const std::string& message)
: std::runtime_error{fmt::format("{}: {}", to_string(loc), message)}
{
}

/*
 * Typing context.
 */

void context::exit_scope(const std::string& name)
{
    if(current_scopes.size() == 0)
    {
        throw type_error(fmt::format("Cannot exit scope '{}': No scope to leave.", name));
    }

    if(current_scopes.back() != name)
    {
        throw type_error(fmt::format("Cannot exit scope '{}': Expected to leave '{}'.", name, current_scopes.back()));
    }

    current_scopes.pop_back();
}

}    // namespace slang::typing
