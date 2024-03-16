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
    return fmt::format("fn {}({}) -> {}", name, slang::utils::join(arg_types, ", "), ret_type);
}

/*
 * Scopes.
 */

std::string scope::to_string() const
{
    std::string repr;
    for(auto& [name, type]: variables)
    {
        repr += fmt::format("[v] name: {}, type: {}\n", name, type);
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

void context::add_variable_type(std::string name, std::string type)
{
    // check for existing names.
    std::string scope_name = slang::utils::join(current_scopes, "::");
    if(scopes.find(scope_name) != scopes.end() && scopes[scope_name].contains(name))
    {
        throw type_error(fmt::format("Name '{}' already exists in scope '{}'.", name, scope_name));
    }

    // check if the type is known.
    // TODO
    if(type != "i32" && type != "f32" && type != "str")
    {
        throw std::runtime_error("context::add_variable_type: Type checking for non-builtin types is not implemented.");
    }

    scopes[scope_name].variables[name] = type;
}

void context::add_function_type(std::string name, std::vector<std::string> arg_types, std::string ret_type)
{
    // check for existing names.
    std::string scope_name = slang::utils::join(current_scopes, "::");
    if(scopes.find(scope_name) != scopes.end() && scopes[scope_name].contains(name))
    {
        throw type_error(fmt::format("Name '{}' already exists in scope '{}'.", name, scope_name));
    }

    // check if all types are known.
    // TODO
    if(ret_type != "i32" && ret_type != "f32" && ret_type != "str" && ret_type != "void")
    {
        throw std::runtime_error("context::add_function_type: Type checking for non-builtin types is not implemented.");
    }

    for(auto& arg_type: arg_types)
    {
        if(arg_type != "i32" && arg_type != "f32" && arg_type != "str")
        {
            throw std::runtime_error("context::add_function_type: Type checking for non-builtin types is not implemented.");
        }
    }

    scopes[scope_name].functions[name] = {name, arg_types, ret_type};
}

std::string context::get_type(const std::string& name) const
{
    std::string scope_name = slang::utils::join(current_scopes, "::");
    auto scope_it = scopes.find(scope_name);
    if(scope_it == scopes.end())
    {
        throw type_error(fmt::format("No scope named '{}'.", scope_name));
    }

    auto var_it = scope_it->second.variables.find(name);
    if(var_it != scope_it->second.variables.end())
    {
        return var_it->second;
    }

    auto func_it = scope_it->second.functions.find(name);
    if(func_it != scope_it->second.functions.end())
    {
        return func_it->second.to_string();
    }

    throw type_error(fmt::format("Name '{}' not found in scope '{}'.", name, scope_name));
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
