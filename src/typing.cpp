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
#include "utils.h"

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

void context::add_type(std::string name, std::string type)
{
    std::string scope_name = slang::utils::join(current_scopes, "::");
    if(variables.find(scope_name) != variables.end() && variables[scope_name].find(name) != variables[scope_name].end())
    {
        throw type_error(fmt::format("Variable '{}' of type '{}' already exists in scope '{}'.", name, type, scope_name));
    }

    variables[scope_name][name] = type;
}

std::string context::get_type(const std::string& name) const
{
    std::string scope_name = slang::utils::join(current_scopes, "::");
    auto scope_it = variables.find(scope_name);
    if(scope_it == variables.end())
    {
        throw type_error(fmt::format("No scope named '{}'.", scope_name));
    }

    auto it = scope_it->second.find(name);
    if(scope_it == variables.end())
    {
        throw type_error(fmt::format("Variable '{}' not defined in scope '{}'.", name, scope_name));
    }

    return it->second;
}

void context::enter_anonymous_scope()
{
    current_scopes.push_back(fmt::format("<anonymous@{}>", anonymous_scope_id));
    ++anonymous_scope_id;
}

void context::exit_anonymous_scope()
{
    if(current_scopes.size() <= 1)    // 1 is the global scope
    {
        throw type_error("Cannot exit anonymous scope: No scope to leave.");
    }

    const auto& s = current_scopes.back();
    if(s.substr(0, 11) != "<anonymous@" || s.back() != '>')
    {
        throw type_error(fmt::format("Cannot exit anonymous scope: Scope id '{}' not anonymous.", s));
    }

    current_scopes.pop_back();
}

void context::exit_scope(const std::string& name)
{
    if(current_scopes.size() <= 1)    // 1 is the global scope
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
