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

std::string to_string(const type& t)
{
    if(t.is_array())
    {
        return fmt::format("[{}; {}]", t.get_base_type().s, t.get_array_length());
    }
    return t.get_base_type().s;
}

std::string to_string(const std::pair<token, std::optional<std::size_t>>& t)
{
    return to_string(type{std::get<0>(t), std::get<1>(t), 0 /* unknown type id */, false});
}

std::string to_string(const std::pair<std::string, std::optional<std::size_t>>& t)
{
    return to_string(type{{std::get<0>(t), {0, 0}}, std::get<1>(t), 0 /* unknown type id */, false});
}

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
    auto transform = [](const type& t)
    {
        return slang::typing::to_string(t);
    };
    return fmt::format("fn {}({}) -> {}",
                       name.s,
                       slang::utils::join(arg_types, {transform}, ", "),
                       slang::typing::to_string(ret_type));
}

/*
 * Scopes.
 */

std::string scope::get_qualified_name() const
{
    std::string qualified_name = name.s;
    for(const scope* s = this->parent; s != nullptr; s = s->parent)
    {
        qualified_name = fmt::format("{}::{}", s->name.s, qualified_name);
    }
    return qualified_name;
}

std::string scope::to_string() const
{
    std::string repr = fmt::format("scope: {}\n------\n", get_qualified_name());
    for(auto& [name, type]: variables)
    {
        repr += fmt::format("[v] name: {}, type: {}\n", name, slang::typing::to_string(type.var_type));
    }
    for(auto& [name, sig]: functions)
    {
        repr += fmt::format("[fn] name: {}, signature: {}\n", name, sig.to_string());
    }
    for(auto& [name, s]: structs)
    {
        repr += fmt::format("[s] name: {}\n    members:\n", name);
        for(auto& [n, t]: s.members)
        {
            repr += fmt::format("    - name: {}, type: {}\n", n.s, slang::typing::to_string(t));
        }
    }

    // remove trailing newline
    repr.pop_back();

    return repr;
}

/*
 * Typing context.
 */

void context::add_import(std::vector<token> path)
{
    if(path.size() == 0)
    {
        throw type_error("Typing context: Cannot add empty import.");
    }

    if(current_scope != &global_scope)
    {
        throw type_error(path[0].location, fmt::format("Import statement can only occur in the global scope."));
    }

    imports.emplace_back(std::move(path));
}

void context::add_variable(token name, type var_type)
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

    current_scope->variables.insert({name.s, {name, std::move(var_type)}});
}

void context::add_function(token name,
                           std::vector<type> arg_types,
                           type ret_type,
                           std::optional<std::string> import_path)
{
    if(current_scope == nullptr)
    {
        throw std::runtime_error("Typing context: No current scope.");
    }

    if(import_path.has_value())
    {
        auto mod_it = imported_functions.find(*import_path);
        if(mod_it == imported_functions.end())
        {
            auto func_type = get_function_type(name, arg_types, ret_type);
            imported_functions.insert({*import_path, {{name.s, {name, std::move(arg_types), std::move(ret_type), std::move(func_type)}}}});
        }
        else
        {
            auto func_it = mod_it->second.find(name.s);
            if(func_it != mod_it->second.end())
            {
                throw type_error(name.location, fmt::format("The module '{}' containing the symbol '{}' already is imported.", *import_path, name.s));
            }
            auto func_type = get_function_type(name, arg_types, ret_type);
            imported_functions[*import_path].insert({name.s, {name, std::move(arg_types), std::move(ret_type), std::move(func_type)}});
        }
    }
    else
    {
        // check for existing names.
        auto tok = current_scope->find(name.s);
        if(tok != std::nullopt)
        {
            throw type_error(name.location, fmt::format("Name '{}' already defined in scope '{}'. The previous definition is here: {}", name.s, current_scope->get_qualified_name(), slang::to_string(tok->location)));
        }

        auto func_type = get_function_type(name, arg_types, ret_type);
        current_scope->functions.insert({name.s, {name, std::move(arg_types), std::move(ret_type), std::move(func_type)}});
    }
}

void context::add_struct(token name, std::vector<std::pair<token, type>> members)
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
    for(auto& [name, type]: members)
    {
        if(!is_builtin_type(type.get_base_type().s))
        {
            if(!has_type(type.get_base_type().s))
            {
                throw type_error(name.location, fmt::format("Struct member has unknown base type '{}'.", type.get_base_type().s));
            }
        }
        else if(type.get_base_type().s == "void")
        {
            throw type_error(name.location, fmt::format("Struct member '{}' cannot have type 'void'.", name.s));
        }
    }

    current_scope->structs[name.s] = {name, std::move(members)};
}

bool context::has_type(const std::string& name) const
{
    if(current_scope == nullptr)
    {
        throw std::runtime_error("Typing context: No current scope.");
    }

    for(const scope* s = current_scope; s != nullptr; s = s->parent)
    {
        if(s->structs.find(name) != s->structs.end())
        {
            return true;
        }
    }

    return false;
}

type context::get_identifier_type(const token& identifier) const
{
    // check if we're accessing a struct.
    if(struct_stack.size() > 0)
    {
        for(auto [n, t]: struct_stack.back()->members)
        {
            if(n.s == identifier.s)
            {
                return t;
            }
        }
    }
    else
    {
        for(scope* s = current_scope; s != nullptr; s = s->parent)
        {
            auto type = s->get_type(identifier.s);
            if(type != std::nullopt)
            {
                return *type;
            }
        }
    }

    throw type_error(identifier.location, fmt::format("Name '{}' not found in current scope.", identifier.s));
}

void context::resolve_types()
{
    // add structs to type map.
    for(auto& s: global_scope.structs)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&s](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(s.first != t.first.get_base_type().s)
                                   {
                                       return false;
                                   }
                                   return !t.first.is_array();
                               });
        if(it == type_map.end())
        {
            auto type_id = generate_type_id();
            type_map.push_back({type{s.second.name, std::nullopt, type_id, false}, type_id});
        }
    }

    // check that all types are resolved.

    // don't resolve built-in types and function types.
    std::vector<type> unresolved;
    std::copy_if(unresolved_types.begin(),
                 unresolved_types.end(),
                 std::back_inserter(unresolved),
                 [](const type& t) -> bool
                 {
                     return !is_builtin_type(t.get_base_type().s) && !t.is_function_type();
                 });
    unresolved_types = std::move(unresolved);    // this clears the moved-from vector.

    // find all unresolved types.
    for(auto& it: unresolved_types)
    {
        if(!has_type(it.get_base_type().s))
        {
            throw type_error(it.get_base_type().location, fmt::format("Function type resolution not implemented (type: '{}').", it.get_base_type().s));
        }
    }

    unresolved_types.clear();
}

type context::get_function_type(const token& name, const std::vector<type>& arg_types, const type& ret_type)
{
    auto transform = [](const type& t) -> std::string
    { return slang::typing::to_string(t); };
    std::string type_string = fmt::format("fn {}({}) -> {}", name.s, slang::utils::join(arg_types, {transform}, ", "),
                                          slang::typing::to_string(ret_type));

    return get_unresolved_type({type_string, name.location}, std::nullopt, true);
}

const function_signature& context::get_function_signature(const token& name) const
{
    if(resolution_scope.size() > 0)
    {
        // check for an import of the scope's name.
        auto import_path = slang::utils::join(resolution_scope, "::");
        auto mod_it = imported_functions.find(import_path);
        if(mod_it != imported_functions.end())
        {
            auto func_it = mod_it->second.find(name.s);
            if(func_it != mod_it->second.end())
            {
                return func_it->second;
            }
        }

        // check if the module was imported.
        for(auto& it: imports)
        {
            auto path = slang::utils::join(it, {[](const token& t) -> std::string
                                                { return t.s; }},
                                           "::");
            if(path == import_path)
            {
                throw type_error(name.location, fmt::format("Function '{}' not found in '{}'.", name.s, import_path));
            }
        }
        throw type_error(name.location, fmt::format("Cannot resolve function '{}' in module '{}', since the module is not imported.", name.s, import_path));
    }
    else
    {
        for(scope* s = current_scope; s != nullptr; s = s->parent)
        {
            auto it = s->functions.find(name.s);
            if(it != s->functions.end())
            {
                return it->second;
            }
        }

        throw type_error(name.location, fmt::format("Function with name '{}' not found in current scope.", name.s));
    }
}

void context::push_resolution_scope(std::string component)
{
    resolution_scope.emplace_back(std::move(component));
}

void context::pop_resolution_scope()
{
    if(resolution_scope.size() > 0)
    {
        resolution_scope.pop_back();
    }
    else
    {
        throw type_error("Cannot pop scope: Scope stack underflow.");
    }
}

std::string context::get_resolution_scope() const
{
    return slang::utils::join(resolution_scope, "::");
}

void context::enter_function_scope(token name)
{
    if(current_scope == nullptr)
    {
        throw type_error(name.location, fmt::format("Cannot enter function scope '{}': No global scope.", name.s));
    }

    if(function_scope != std::nullopt)
    {
        throw type_error(name.location, fmt::format("Nested functions are not allowed."));
    }
    function_scope = name;

    // check if the scope already exists.
    auto it = std::find_if(current_scope->children.begin(), current_scope->children.end(),
                           [&name](const scope& s) -> bool
                           { return s.name.s == name.s; });
    if(it != current_scope->children.end())
    {
        current_scope = &(*it);
    }
    else
    {
        current_scope->children.emplace_back(std::move(name), current_scope);
        current_scope = &current_scope->children.back();
    }
}

void context::exit_function_scope(const token& name)
{
    if(current_scope == nullptr)
    {
        throw type_error(name.location, fmt::format("Cannot exit scope '{}': No scope to leave.", name.s));
    }

    if(current_scope->parent == nullptr)
    {
        throw type_error(name.location, fmt::format("Cannot exit scope '{}': No scope to leave.", name.s));
    }

    if(current_scope->name.s != name.s)
    {
        throw type_error(name.location, fmt::format("Cannot exit scope '{}': Expected to exit scope '{}'.", name.s, current_scope->name.s));
    }

    function_scope = std::nullopt;
    current_scope = current_scope->parent;
}

std::optional<function_signature> context::get_current_function() const
{
    if(function_scope == std::nullopt)
    {
        return std::nullopt;
    }

    return get_function_signature(*function_scope);
}

void context::enter_anonymous_scope(token_location loc)
{
    token anonymous_scope;
    anonymous_scope.location = std::move(loc);
    anonymous_scope.s = std::move(fmt::format("<anonymous@{}>", anonymous_scope_id));
    ++anonymous_scope_id;

    // check if the scope already exists.
    auto it = std::find_if(current_scope->children.begin(), current_scope->children.end(),
                           [&anonymous_scope](const scope& s) -> bool
                           { return s.name.s == anonymous_scope.s; });
    if(it != current_scope->children.end())
    {
        // this should never happen.
        throw type_error(anonymous_scope.location, fmt::format("Cannot enter anonymous scope: Name '{}' already exists.", anonymous_scope.s));
    }
    else
    {
        current_scope->children.emplace_back(std::move(anonymous_scope), current_scope);
        current_scope = &current_scope->children.back();
    }
}

void context::exit_anonymous_scope()
{
    if(current_scope->parent == nullptr)
    {
        throw type_error(current_scope->name.location, "Cannot exit anonymous scope: No scope to leave.");
    }

    if(current_scope->name.s.substr(0, 11) != "<anonymous@" || current_scope->name.s.back() != '>')
    {
        throw type_error(current_scope->name.location, fmt::format("Cannot exit anonymous scope: Scope id '{}' not anonymous.", current_scope->name.s));
    }

    current_scope = current_scope->parent;
}

const token& context::get_scope_name() const
{
    if(current_scope == nullptr)
    {
        throw std::runtime_error("Typing context: No current scope.");
    }

    return current_scope->name;
}

const struct_definition* context::get_struct_definition(token_location loc, const std::string& name) const
{
    for(scope* s = current_scope; s != nullptr; s = s->parent)
    {
        auto it = s->structs.find(name);
        if(it != s->structs.end())
        {
            return &it->second;
        }
    }

    throw type_error(loc, fmt::format("Unknown struct '{}'.", name));
}

void context::push_struct_definition(const struct_definition* s)
{
    struct_stack.push_back(s);
}

void context::pop_struct_definition()
{
    if(struct_stack.size() == 0)
    {
        throw std::runtime_error("Typing context: Struct stack is empty.");
    }

    struct_stack.pop_back();
}

std::string context::to_string() const
{
    std::string ret = "Imports:\n";
    for(auto& it: imports)
    {
        auto transform = [](const token& t) -> std::string
        { return t.s; };
        ret += fmt::format("* {}\n", slang::utils::join(it, {transform}, "::"));
    }

    return fmt::format("{}\n{}", ret, global_scope.to_string());
}

}    // namespace slang::typing
