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

#include "type.h"
#include "typing.h"
#include "utils.h"

namespace ty = slang::typing;

namespace slang::typing
{

/*
 * type implementation.
 */

type::type(const token& base, type_class cls, std::optional<std::uint64_t> type_id)
: location{base.location}
, cls{cls}
, type_id{type_id}
{
    if(cls == type_class::tc_array)
    {
        components = {std::make_shared<type>(base, type_class::tc_plain, std::nullopt)};
    }
    else
    {
        name = std::make_optional<std::string>(base.s);
    }
}

bool type::operator==(const type& other) const
{
    if(!type_id.has_value() || !other.type_id.has_value())
    {
        throw type_error(fmt::format(
          "Comparison of types '{}' ({}) and '{}' ({}).",
          to_string(),
          (type_id.has_value() ? "resolved" : "unresolved"),
          other.to_string(),
          (other.type_id.has_value() ? "resolved" : "unresolved")));
    }

    return type_id.value() == other.type_id.value();
}

bool type::operator!=(const type& other) const
{
    return !(*this == other);
}

type* type::get_element_type()
{
    if(!is_array())
    {
        auto error_string = name.has_value()
                              ? fmt::format("Cannot get element type for '{}'.", *name)
                              : std::string{"Cannot get element type."};
        throw type_error(location, error_string);
    }

    if(components.size() != 1)
    {
        auto error_string = name.has_value()
                              ? fmt::format("Inconsistent component count for array type '{}' ({} components, expected 1).", *name, components.size())
                              : fmt::format("Inconsistent component count for array type ({} components, expected 1).", components.size());
        throw type_error(location, error_string);
    }

    return components[0].get();
}

const type* type::get_element_type() const
{
    if(!is_array())
    {
        auto error_string = name.has_value()
                              ? fmt::format("Cannot get element type for '{}'.", *name)
                              : std::string{"Cannot get element type."};
        throw type_error(location, error_string);
    }

    if(components.size() != 1)
    {
        auto error_string = name.has_value()
                              ? fmt::format("Inconsistent component count for array type '{}' ({} components, expected 1).", *name, components.size())
                              : fmt::format("Inconsistent component count for array type ({} components, expected 1).", components.size());
        throw type_error(location, error_string);
    }

    return components[0].get();
}

const std::vector<std::shared_ptr<type>>& type::get_signature() const
{
    if(!is_function_type())
    {
        auto error_string = name.has_value()
                              ? fmt::format("Cannot get signature for non-function type '{}'.", *name)
                              : std::string{"Cannot get signature for non-function type."};
        throw type_error(location, error_string);
    }

    if(components.size() == 0)
    {
        auto error_string = name.has_value()
                              ? fmt::format("Inconsistent component count for function signature '{}' (0 components, expected at least 1).", *name)
                              : std::string{"Inconsistent component count for function signature (0 components, expected at least 1)."};
        throw type_error(location, error_string);
    }

    return components;
}

std::uint64_t type::get_type_id() const
{
    if(!type_id.has_value())
    {
        auto error_string = name.has_value()
                              ? fmt::format("Unresolved type '{}'.", *name)
                              : std::string{"Unresolved type."};
        throw type_error(location, error_string);
    }

    return *type_id;
}

/*
 * type to string conversions.
 */

std::string to_string(const type& t)
{
    return t.to_string();
}

std::string to_string(const std::pair<token, bool>& t)
{
    return to_string(type{
      std::get<0>(t),
      std::get<1>(t) ? type_class::tc_array : type_class::tc_plain,
      std::nullopt});
}

std::string to_string(const std::pair<std::string, bool>& t)
{
    return to_string(type{
      {std::get<0>(t), {0, 0}},
      std::get<1>(t) ? type_class::tc_array : type_class::tc_plain,
      std::nullopt});
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
        return ty::to_string(t);
    };
    return fmt::format("fn {}({}) -> {}",
                       name.s,
                       slang::utils::join(arg_types, {transform}, ", "),
                       ty::to_string(ret_type));
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
        repr += fmt::format("[v]  name: {}, type: {}\n", name, ty::to_string(type.var_type));
    }
    for(auto& [name, sig]: functions)
    {
        repr += fmt::format("[fn] name: {}, signature: {}\n", name, sig.to_string());
    }
    for(auto& [name, s]: structs)
    {
        repr += fmt::format("[s]  name: {}\n    members:\n", name);
        for(auto& [n, t]: s.members)
        {
            repr += fmt::format("     - name: {}, type: {}\n", n.s, ty::to_string(t));
        }
    }

    // remove trailing newline
    repr.pop_back();

    return repr;
}

/*
 * Typing context.
 */

void context::add_base_type(std::string name, bool is_reference_type)
{
    auto it = std::find_if(type_map.begin(), type_map.end(),
                           [&name](const std::pair<type, std::uint64_t>& t) -> bool
                           {
                               return name == ty::to_string(t.first);
                           });
    if(it != type_map.end())
    {
        throw type_error(fmt::format("Type '{}' already exists.", name));
    }

    if(std::find_if(base_types.begin(), base_types.end(),
                    [&name](const std::pair<std::string, bool>& v) -> bool
                    { return v.first == name; })
       != base_types.end())
    {
        throw type_error(fmt::format("Inconsistent type context: Type '{}' exists in base types, but not in type map.", name));
    }

    auto type_id = generate_type_id();
    type_map.push_back({type{{name, {0, 0}}, type_class::tc_plain, type_id}, type_id});

    base_types.push_back(std::make_pair(std::move(name), is_reference_type));
}

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
    for(auto& [member_name, member_type]: members)
    {
        auto type_string = member_type.to_string();

        if(!is_builtin_type(type_string))
        {
            if(!has_type(member_type)   /* custom types */
               && type_string != name.s /* currently declared type*/
            )
            {
                throw type_error(member_name.location, fmt::format("Struct member has unknown base type '{}'.", type_string));
            }
        }
        else if(type_string == "void")
        {
            throw type_error(member_name.location, fmt::format("Struct member '{}' cannot have type 'void'.", member_name.s));
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

    // search type map.
    auto it = std::find_if(type_map.begin(), type_map.end(),
                           [&name](const std::pair<type, std::uint64_t>& t) -> bool
                           {
                               return name == t.first.to_string();
                           });
    if(it != type_map.end())
    {
        return true;
    }

    // search scopes.
    for(const scope* s = current_scope; s != nullptr; s = s->parent)
    {
        if(s->structs.find(name) != s->structs.end())
        {
            return true;
        }
    }

    return false;
}

bool context::has_type(const type& ty) const
{
    return has_type(ty.to_string());
}

bool context::is_reference_type(const std::string& name) const
{
    // check base types.
    auto base_it = std::find_if(base_types.begin(), base_types.end(),
                                [&name](const std::pair<std::string, bool>& v) -> bool
                                {
                                    return v.first == name;
                                });
    if(base_it != base_types.end())
    {
        return base_it->second;
    }

    // all other types are references.
    // FIXME This checks the base types twice.
    return has_type(name);
}

bool context::is_reference_type(const type& t) const
{
    return is_reference_type(t.to_string());
}

type context::get_identifier_type(const token& identifier) const
{
    // check if we're accessing a struct.
    std::string err;
    if(struct_stack.size() > 0)
    {
        for(auto [n, t]: struct_stack.back()->members)
        {
            if(n.s == identifier.s)
            {
                return t;
            }
        }

        err = fmt::format("Name '{}' not found in struct '{}'.", identifier.s, struct_stack.back()->name.s);
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

        err = fmt::format("Name '{}' not found in scope '{}'.", identifier.s, current_scope->name.s);
    }

    throw type_error(identifier.location, err);
}

type context::get_type(const std::string& name, bool array)
{
    auto it = std::find_if(type_map.begin(), type_map.end(),
                           [&name, &array](const std::pair<type, std::uint64_t>& t) -> bool
                           {
                               if(array != t.first.is_array())
                               {
                                   return false;
                               }

                               if(array && t.first.is_array())
                               {
                                   const type* element_type = t.first.get_element_type();
                                   return name == ty::to_string(*element_type);
                               }

                               return name == ty::to_string(t.first);
                           });
    if(it != type_map.end())
    {
        return it->first;
    }

    // for arrays, also search for the base type.
    if(array)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&name](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(t.first.is_array())
                                   {
                                       return false;
                                   }

                                   return name == ty::to_string(t.first);
                               });
        if(it != type_map.end())
        {
            // add the array type to the type map.
            auto type_id = generate_type_id();
            type_map.push_back({type{
                                  token{name, {0, 0}},
                                  array ? type_class::tc_array : type_class::tc_plain,
                                  type_id},
                                type_id});
            return type_map.back().first;
        }
    }

    throw type_error(fmt::format("Unknown type '{}'.", name));
}

type context::get_unresolved_type(token name, type_class cls)
{
    auto it = std::find_if(unresolved_types.begin(), unresolved_types.end(),
                           [&name, &cls](const type& t) -> bool
                           {
                               if(cls == type_class::tc_array && t.is_array())
                               {
                                   const type* element_type = t.get_element_type();
                                   return element_type != nullptr && (name.s == ty::to_string(*element_type));
                               }

                               return name.s == ty::to_string(t);
                           });
    if(it != unresolved_types.end())
    {
        return *it;
    }

    unresolved_types.push_back(type::make_unresolved(std::move(name), cls));
    return unresolved_types.back();
}

bool context::is_convertible(const type& from, const type& to) const
{
    // Only conversions from array types to @array are allowed.
    return from.is_array() && (!to.is_array() && to.to_string() == "@array");
}

void context::resolve(type& ty)
{
    if(ty.is_resolved())
    {
        return;
    }

    // check if array elements are resolved.
    if(ty.is_array())
    {
        type* element_type = ty.get_element_type();
        if(!element_type->is_resolved())
        {
            resolve(*element_type);
        }
    }

    // resolve type.
    auto it = std::find_if(type_map.begin(), type_map.end(),
                           [&ty](const std::pair<type, std::uint64_t>& t) -> bool
                           {
                               return ty.to_string() == t.first.to_string();
                           });
    if(it == type_map.end())
    {
        // add array types.
        if(ty.is_array())
        {
            auto type_id = generate_type_id();
            ty.set_type_id(type_id);
            type_map.push_back({ty, type_id});
            return;
        }

        throw type_error(ty.get_location(), fmt::format("Cannot resolve type '{}'.", ty.to_string()));
    }

    ty.set_type_id(it->second);
}

void context::resolve_types()
{
    // add structs to type map.
    for(auto& s: global_scope.structs)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&s](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(s.first != t.first.to_string())
                                   {
                                       return false;
                                   }
                                   return !t.first.is_array();
                               });
        if(it == type_map.end())
        {
            auto type_id = generate_type_id();
            type_map.push_back({type{s.second.name, type_class::tc_plain, type_id}, type_id});
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
                     return !is_builtin_type(t.to_string()) && !t.is_function_type();
                 });
    unresolved_types = std::move(unresolved);    // this clears the moved-from vector.

    // find all unresolved types.
    for(auto& it: unresolved_types)
    {
        resolve(it);
    }
    unresolved_types.clear();

    // propagate type resolutions to functions.
    for(auto& [f, sig]: global_scope.functions)
    {
        for(auto& arg: sig.arg_types)
        {
            resolve(arg);
        }

        resolve(sig.ret_type);
    }

    // propagate type resolutions to structs.
    for(auto& s: global_scope.structs)
    {
        for(auto& [member_name, member_type]: s.second.members)
        {
            resolve(member_type);
        }
    }
}

type context::get_function_type(const token& name, const std::vector<type>& arg_types, const type& ret_type)
{
    auto transform = [](const type& t) -> std::string
    { return ty::to_string(t); };
    std::string type_string = fmt::format("fn {}({}) -> {}", name.s, slang::utils::join(arg_types, {transform}, ", "),
                                          ty::to_string(ret_type));

    return get_unresolved_type({type_string, name.location}, type_class::tc_function);
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
    anonymous_scope.s = fmt::format("<anonymous@{}>", anonymous_scope_id);
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

    ret += "\nType map:\n";
    for(auto& it: type_map)
    {
        ret += fmt::format("  {}, {}\n", it.first.to_string(), it.second);
    }

    return fmt::format("{}\n{}", ret, global_scope.to_string());
}

}    // namespace slang::typing
