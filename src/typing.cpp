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

std::string scope::get_qualified_name() const
{
    std::string qualified_name;
    for(const scope* s = this; s != nullptr; s = s->parent)
    {
        qualified_name = fmt::format("{}::{}", s->name, qualified_name);
    }
    return qualified_name;
}

std::string scope::to_string() const
{
    std::string repr = fmt::format("scope: {}\n------\n", get_qualified_name());
    for(auto& [name, type]: variables)
    {
        repr += fmt::format("[v] name: {}, type: {}\n", name, type.type.s);
    }
    for(auto& [name, sig]: functions)
    {
        repr += fmt::format("[fn] name: {}, signature: {}\n", name, sig.to_string());
    }

    // remove trailing newline
    repr.pop_back();

    return repr;
}

/*
 * Typing context.
 */

void context::add_variable_type(token name, token type)
{
    if(current_scope == nullptr)
    {
        throw std::runtime_error("Typing context: No current scope.");
    }

    // check for existing names.
    auto tok = current_scope->find(name.s);
    if(tok != std::nullopt)
    {
        throw type_error(name.location, fmt::format("Name '{}' already defined in scope '{}'. The previous definition is here: {}", name.s, current_scope->get_qualified_name(), slang::to_string(tok->location)));
    }

    // check if the type is known.
    if(type.s != "i32" && type.s != "f32" && type.s != "str")
    {
        if(!has_type(type.s))
        {
            throw type_error(type.location, fmt::format("Unknown type '{}'.", type.s));
        }
    }

    current_scope->variables[name.s] = {name, std::move(type)};
}

void context::add_function_type(token name, std::vector<token> arg_types, token ret_type)
{
    if(current_scope == nullptr)
    {
        throw std::runtime_error("Typing context: No current scope.");
    }

    // check for existing names.
    auto tok = current_scope->find(name.s);
    if(tok != std::nullopt)
    {
        throw type_error(name.location, fmt::format("Name '{}' already defined in scope '{}'. The previous definition is here: {}", name.s, current_scope->get_qualified_name(), slang::to_string(tok->location)));
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

    current_scope->functions[name.s] = {name, std::move(arg_types), std::move(ret_type)};
}

bool context::has_type(const std::string& name) const
{
    // TODO
    return false;
}

std::string context::get_type(const token& name) const
{
    for(scope* s = current_scope; s != nullptr; s = s->parent)
    {
        auto type = s->get_type(name.s);
        if(type != std::nullopt)
        {
            return *type;
        }
    }

    throw type_error(name.location, fmt::format("Name '{}' not found in current scope.", name.s));
}

void context::enter_anonymous_scope(token_location loc)
{
    std::string name = fmt::format("<anonymous@{}>", anonymous_scope_id);
    ++anonymous_scope_id;

    // check if the scope already exists.
    auto it = std::find_if(current_scope->children.begin(), current_scope->children.end(),
                           [](const scope& s) -> bool
                           { return true; });
    if(it != current_scope->children.end())
    {
        // this should never happen.
        throw type_error(loc, fmt::format("Cannot enter anonymous scope: Name '{}' already exists.", name));
    }
    else
    {
        current_scope->children.emplace_back(std::move(loc), std::move(name), current_scope);
        current_scope = &current_scope->children.back();
    }
}

void context::exit_anonymous_scope()
{
    if(current_scope->parent == nullptr)
    {
        throw type_error(current_scope->loc, "Cannot exit anonymous scope: No scope to leave.");
    }

    if(current_scope->name.substr(0, 11) != "<anonymous@" || current_scope->name.back() != '>')
    {
        throw type_error(current_scope->loc, fmt::format("Cannot exit anonymous scope: Scope id '{}' not anonymous.", current_scope->name));
    }

    current_scope = current_scope->parent;
}

void context::exit_scope(const std::string& name)
{
    if(current_scope->parent == nullptr)
    {
        throw type_error(current_scope->loc, "Cannot exit anonymous scope: No scope to leave.");
    }

    if(current_scope->name != name)
    {
        throw type_error(current_scope->loc, fmt::format("Cannot exit scope '{}': Expected to leave '{}'.", name, current_scope->name));
    }

    current_scope = current_scope->parent;
}

std::string context::to_string() const
{
    return global_scope.to_string();
}

}    // namespace slang::typing
