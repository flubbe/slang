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
 * Function signature.
 */

std::string function_signature::to_string() const
{
    auto transform = [](const token& t)
    { return t.s; };
    return fmt::format("fn {}({}) -> {}", name.s, slang::utils::join(arg_types, {transform}, ", "), ret_type.s);
}

/*
 * Scopes.
 */

std::string scope::to_string() const
{
    std::string repr;
    for(auto& [name, type]: variables)
    {
        repr += fmt::format("[v] name: {}, type: {}\n", name, type.type.s);
    }
    for(auto& [name, sig]: functions)
    {
        repr += fmt::format("[fn] name: {}, signature: {}\n", name, sig.to_string());
    }

    // remove trailing newline
    if(variables.size() > 0 || functions.size() > 0)
    {
        repr.pop_back();
    }

    return repr;
}

/*
 * Typing context.
 */

void context::add_variable_type(token name, token type)
{
    auto transform = [](const std::pair<std::optional<token_location>, std::string>& v) -> std::string
    { return v.second; };

    // check for existing names.
    std::string scope_name = slang::utils::join(current_scopes, {transform}, "::");
    if(scopes.find(scope_name) != scopes.end() && scopes[scope_name].contains(name.s))
    {
        throw type_error(name.location, fmt::format("Name '{}' already exists in scope '{}'.", name.s, scope_name));
    }

    // check if the type is known.
    if(type.s != "i32" && type.s != "f32" && type.s != "str")
    {
        if(!has_type(type.s))
        {
            throw type_error(type.location, fmt::format("Unknown type '{}'.", type.s));
        }
    }

    scopes[scope_name].variables[name.s] = {name, std::move(type)};
}

void context::add_function_type(token name, std::vector<token> arg_types, token ret_type)
{
    auto transform = [](const std::pair<std::optional<token_location>, std::string>& v) -> std::string
    { return v.second; };

    // check for existing names.
    std::string scope_name = slang::utils::join(current_scopes, {transform}, "::");
    if(scopes.find(scope_name) != scopes.end() && scopes[scope_name].contains(name.s))
    {
        throw type_error(name.location, fmt::format("Name '{}' already exists in scope '{}'.", name.s, scope_name));
    }

    // check if all types are known.
    if(ret_type.s != "i32" && ret_type.s != "f32" && ret_type.s != "str" && ret_type.s != "void")
    {
        if(!has_type(ret_type.s))
        {
            throw type_error(ret_type.location, fmt::format("Unknown return type '{}'.", ret_type.s));
        }
    }

    for(auto& arg_type: arg_types)
    {
        if(arg_type.s != "i32" && arg_type.s != "f32" && arg_type.s != "str")
        {
            if(!has_type(arg_type.s))
            {
                throw type_error(arg_type.location, fmt::format("Unknown argument type '{}'.", arg_type.s));
            }
        }
    }

    scopes[scope_name].functions[name.s] = {name, std::move(arg_types), std::move(ret_type)};
}

bool context::has_type(const std::string& name) const
{
    auto transform = [](const std::pair<std::optional<token_location>, std::string>& v) -> std::string
    { return v.second; };

    // check all scopes, from inner-most to outer-most.
    auto scope_copy = current_scopes;
    while(scope_copy.size() > 0)
    {
        std::string scope_name = slang::utils::join(scope_copy, {transform}, "::");

        auto it = scopes.find(scope_name);
        if(it != scopes.end() && it->second.contains(name))
        {
            return true;
        }

        scope_copy.pop_back();
    }

    return false;
}

std::string context::get_type(const token& name) const
{
    auto transform = [](const std::pair<std::optional<token_location>, std::string>& v) -> std::string
    { return v.second; };

    std::string scope_name = slang::utils::join(current_scopes, {transform}, "::");
    auto scope_it = scopes.find(scope_name);
    if(scope_it == scopes.end())
    {
        throw type_error(name.location, fmt::format("No scope named '{}'.", scope_name));
    }

    auto var_it = scope_it->second.variables.find(name.s);
    if(var_it != scope_it->second.variables.end())
    {
        return var_it->second.type.s;
    }

    auto func_it = scope_it->second.functions.find(name.s);
    if(func_it != scope_it->second.functions.end())
    {
        return func_it->second.to_string();
    }

    throw type_error(name.location, fmt::format("Name '{}' not found in scope '{}'.", name.s, scope_name));
}

void context::enter_anonymous_scope(token_location loc)
{
    current_scopes.emplace_back(std::make_pair<token_location, std::string>(std::move(loc), fmt::format("<anonymous@{}>", anonymous_scope_id)));
    ++anonymous_scope_id;
}

void context::exit_anonymous_scope()
{
    if(current_scopes.size() <= 1)    // 1 is the global scope
    {
        throw type_error("Cannot exit anonymous scope: No scope to leave.");
    }

    const auto& s = current_scopes.back();
    if(s.second.substr(0, 11) != "<anonymous@" || s.second.back() != '>')
    {
        throw type_error(fmt::format("Cannot exit anonymous scope: Scope id '{}' not anonymous.", s.second));
    }

    current_scopes.pop_back();
}

void context::exit_scope(const std::string& name)
{
    if(current_scopes.size() <= 1)    // 1 is the global scope
    {
        throw type_error(fmt::format("Cannot exit scope '{}': No scope to leave.", name));
    }

    if(current_scopes.back().second != name)
    {
        throw type_error(fmt::format("Cannot exit scope '{}': Expected to leave '{}'.", name, current_scopes.back().second));
    }

    current_scopes.pop_back();
}

std::string context::to_string() const
{
    std::string repr;

    for(auto& [name, scope]: scopes)
    {
        repr += fmt::format("scope: {}\n------\n{}\n\n", name, scope.to_string());
    }

    // remove trailing newlines
    if(scopes.size() > 0)
    {
        repr.pop_back();
        repr.pop_back();
    }

    return repr;
}

}    // namespace slang::typing
