/**
 * slang - a simple scripting language.
 *
 * type system context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "token.h"
#include "type.h"

namespace slang::typing
{

/** Type errors. */
class type_error : public std::runtime_error
{
public:
    /**
     * Construct a type_error.
     *
     * @note Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    type_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a type_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    type_error(const slang::token_location& loc, const std::string& message);
};

/** A variable type. */
struct variable_type
{
    /** The variable's name. */
    token name;

    /** The variable's type, given as `(base_type, optional_array_length)`. */
    type var_type;

    /** No default constructor. */
    variable_type() = delete;

    /** Default copy and move constructors. */
    variable_type(const variable_type&) = default;
    variable_type(variable_type&&) = default;

    /** Default assignments. */
    variable_type& operator=(const variable_type&) = default;
    variable_type& operator=(variable_type&&) = default;

    /**
     * Construct a new variable type.
     *
     * @param name The variable's name.
     * @param type The variable's type.
     */
    variable_type(token name, type var_type)
    : name{std::move(name)}
    , var_type{std::move(var_type)}
    {
    }
};

/** A function signature. */
struct function_signature
{
    /** Name of the function. */
    token name;

    /** Argument types. */
    std::vector<type> arg_types;

    /** Return type. */
    type ret_type;

    /** The function type (e.g. a combination of `ret_type`, `name` and `arg_types`, together with a type id). */
    type func_type;

    /** No default constructor. */
    function_signature() = delete;

    /** Default copy and move constructors. */
    function_signature(const function_signature&) = default;
    function_signature(function_signature&&) = default;

    /** Default assignments. */
    function_signature& operator=(const function_signature&) = default;
    function_signature& operator=(function_signature&&) = default;

    /**
     * Construct a new function_signature.
     *
     * @param name The function's name.
     * @param arg_types The function's argument types.
     * @param ret_type The function's return type.
     * @param func_type The function's type. E.g. a combination of return type, names and arguments, together with a type id.
     */
    function_signature(token name,
                       std::vector<type> arg_types,
                       type ret_type,
                       type func_type)
    : name{std::move(name)}
    , arg_types{std::move(arg_types)}
    , ret_type{std::move(ret_type)}
    , func_type{std::move(func_type)}
    {
    }

    /** Return a string representation of the signature. */
    std::string to_string() const;
};

/** A struct. */
struct struct_definition
{
    /** The struct's name. */
    token name;

    /** The struct's members as `(name, type)` pairs. */
    std::vector<std::pair<token, type>> members;

    /** Default constructors. */
    struct_definition() = default;
    struct_definition(const struct_definition&) = default;
    struct_definition(struct_definition&&) = default;

    /** Default assignments. */
    struct_definition& operator=(const struct_definition&) = default;
    struct_definition& operator=(struct_definition&&) = default;

    /**
     * Construct a struct definition.
     *
     * @param name The struct's name.
     * @param members The struct's members.
     */
    struct_definition(token name, std::vector<std::pair<token, type>> members)
    : name{std::move(name)}
    , members{std::move(members)}
    {
    }
};

/** A scope. */
struct scope
{
    /** The scope's name. */
    token name;

    /** Parent scope (if any). */
    scope* parent = nullptr;

    /** Child scopes. */
    std::list<scope> children;

    /** Variables. */
    std::unordered_map<std::string, variable_type> variables;

    /** Functions. */
    std::unordered_map<std::string, function_signature> functions;

    /** Structs. */
    std::unordered_map<std::string, struct_definition> structs;

    /** Default constructors. */
    scope() = default;
    scope(const scope&) = default;
    scope(scope&&) = default;

    /** Default assignment. */
    scope& operator=(const scope&) = default;
    scope& operator=(scope&&) = default;

    /**
     * Construct a scope from location and name.
     *
     * @param loc The scope location.
     * @param name The scope name.
     */
    scope(token_location loc, std::string scope_name)
    {
        name.s = std::move(scope_name);
        name.location = std::move(loc);
    }

    /**
     * Construct a scope from name and parent.
     *
     * @param name The scope's name.
     * @param parent The parent scope, if any.
     */
    scope(token name, scope* parent)
    : name{std::move(name)}
    , parent{parent}
    {
    }

    /**
     * Check whether this scope contains a name or a type.
     *
     * @param name The name to check for.
     * @returns True if the name exists in this scope or false if not.
     */
    bool contains(const std::string& name) const
    {
        return variables.find(name) != variables.end()
               || functions.find(name) != functions.end()
               || structs.find(name) != structs.end();
    }

    /**
     * Find the token of a name in this scope.
     *
     * @param name The name to find.
     * @returns If the name is found, returns its token, and std::nullopt otherwise.
     */
    std::optional<token> find(const std::string& name) const
    {
        auto var_it = variables.find(name);
        if(var_it != variables.end())
        {
            return {var_it->second.name};
        }

        auto func_it = functions.find(name);
        if(func_it != functions.end())
        {
            return {func_it->second.name};
        }

        auto struct_it = structs.find(name);
        if(struct_it != structs.end())
        {
            return {struct_it->second.name};
        }

        return std::nullopt;
    }

    /**
     * Get the type of a name in this scope.
     *
     * @param name The name to find.
     * @returns If the name is found, returns its type and `std::nullopt` otherwise.
     */
    std::optional<type> get_type(const std::string& name) const
    {
        auto var_it = variables.find(name);
        if(var_it != variables.end())
        {
            return var_it->second.var_type;
        }

        auto func_it = functions.find(name);
        if(func_it != functions.end())
        {
            return func_it->second.func_type;
        }

        return std::nullopt;
    }

    /** Get the qualified scope name. */
    std::string get_qualified_name() const;

    /** Get a string representation of the scope. */
    std::string to_string() const;
};

/** Type system context. */
class context
{
    /** The global scope. */
    scope global_scope = {{1, 1}, "<global>"};

    /** The current scope. */
    scope* current_scope = &global_scope;

    /** The current function scope. */
    std::optional<token> function_scope;

    /** The scope we're resolving the names in. Clear this to use the default scope. */
    std::vector<std::string> resolution_scope;

    /** The current anonymous scope id. */
    std::size_t anonymous_scope_id = 0;

    /** Imported modules. */
    std::vector<std::vector<token>> imports;

    /** Imported functions, indexed by (import_path, function_name). */
    std::unordered_map<std::string, std::unordered_map<std::string, function_signature>> imported_functions;

    /** Struct/type stack, for member/type lookups. */
    std::vector<const struct_definition*> struct_stack;

    /** Unresolved types. */
    std::vector<type> unresolved_types;

    /** Map of types to type ids. */
    std::vector<std::pair<type, std::uint64_t>> type_map;

    /** The next type id to use. */
    std::uint64_t next_type_id = 0;

    /** Whether we are parsing a native function declaration. Enables untyped array parsing. */
    bool parsing_native{false};

    /** Directive stack with entries `(name, restore_function)`. */
    std::vector<std::pair<token, std::function<void(void)>>> directive_stack;

    /**
     * Generate a unique type id.
     *
     * @returns A unique type id.
     */
    std::uint64_t generate_type_id()
    {
        ++next_type_id;
        return next_type_id - 1;
    }

    /**
     * Add a base type.
     *
     * @param name The type's name.
     * @throws Throws a `type_error` if the type already exists.
     */
    void add_base_type(std::string name)
    {
        add_type({std::move(name), {0, 0}});
    }

    /**
     * Add a type to the context.
     *
     * @param name The type's name.
     * @throws Throws a `type_error` if the type already exists.
     */
    void add_type(token name)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&name](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(name.s != t.first.get_base_type().s)
                                   {
                                       return false;
                                   }
                                   return !t.first.is_array();
                               });
        if(it != type_map.end())
        {
            throw type_error(fmt::format("Base type '{}' already exists.", name.s));
        }

        auto type_id = generate_type_id();
        type_map.push_back({type{std::move(name), false, type_id, false}, type_id});
    }

public:
    /** Default constructor. */
    context()
    {
        //  Initialize the default types `void`, `i32`, `f32`, `str`.
        add_base_type("void");
        add_base_type("i32");
        add_base_type("f32");
        add_base_type("str");

        // Add array type.
        add_struct(token{"<array>", {0, 0}}, {std::make_pair(token{"length", {0, 0}}, get_type("i32", false))});
    }
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = default;
    context& operator=(context&&) = default;

    /**
     * Add an import to the context.
     *
     * @throws A type_error if the import is not in the global scope.
     * @throws A type_error if the import is already added.
     *
     * @param path The import path.
     */
    void add_import(std::vector<token> path);

    /**
     * Add a variable to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if the given type is unknown.
     *
     * @param name The variable's name.
     * @param var_type The variable's type.
     */
    void add_variable(token name, type var_type);

    /**
     * Add a function to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The function's name.
     * @param arg_types The functions's argument types.
     * @param ret_type The function's return type.
     * @param import_path The import path for the function.
     */
    void add_function(token name,
                      std::vector<type> arg_types,
                      type ret_type,
                      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a struct to the context.
     *
     * @throws A type_error if the type already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The name of the type.
     * @param members The members, given as pairs `(name, type)`.
     */
    void add_struct(token name, std::vector<std::pair<token, type>> members);

    /**
     * Check if the context has a specific type.
     *
     * @param name The type's name.
     * @returns True if the type exists within the current scope.
     */
    bool has_type(const std::string& name) const;

    /**
     * Get the type for an identifier.
     *
     * @param identifier The identifier to resolve the type for.
     * @returns The resolved type.
     * @throws Throws a `type_error` if the identifier's type could not be resolved.
     */
    type get_identifier_type(const token& identifier) const;

    /**
     * Get the type id for a name.
     *
     * @param name The type name.
     * @param array Whether this is an array type.
     * @throws Throws a `type_error` if the type is not known.
     */
    std::uint64_t get_type_id(const token& name, bool array)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&name, &array](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(name.s != t.first.get_base_type().s)
                                   {
                                       return false;
                                   }

                                   return (array && t.first.is_array())
                                          || (!array && !t.first.is_array());
                               });
        if(it != type_map.end())
        {
            return it->second;
        }

        // add array types as necessary.
        it = std::find_if(type_map.begin(), type_map.end(),
                          [&name, &array](const std::pair<type, std::uint64_t>& t) -> bool
                          {
                              return name.s == t.first.get_base_type().s;
                          });
        if(it == type_map.end())
        {
            throw type_error(name.location, fmt::format("Unknown type '{}'.", name.s));
        }

        return get_type(name, array).get_type_id();
    }

    /**
     * Get the type for a name and an optional array length.
     *
     * Adds the array type to the type map if the base type exists.
     *
     * @param name The type name.
     * @param array Whether the type is an array type.
     * @returns An existing type.
     * @throws A `type_error` if the type cnould not be resolved.
     */
    type get_type(const token& name, bool array)
    {
        // search for an exact match.
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&name, &array](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(name.s != t.first.get_base_type().s)
                                   {
                                       return false;
                                   }
                                   return (array && t.first.is_array())
                                          || (!array && !t.first.is_array());
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
                                       if(name.s != t.first.get_base_type().s)
                                       {
                                           return false;
                                       }
                                       if(t.first.is_array())
                                       {
                                           return false;
                                       }
                                       return true;
                                   });
            if(it != type_map.end())
            {
                // add the array type to the type map.
                auto type_id = generate_type_id();
                type_map.push_back({type{name, array, type_id, false}, type_id});
                return type_map.back().first;
            }
        }

        throw type_error(name.location, fmt::format("Unknown type '{}'.", name.s));
    }

    /**
     * Get the type for a name.
     *
     * Adds the array type to the type map if the base type exists.
     *
     * @param name The type name.
     * @param array Whether the type is an array.
     * @returns An existing type.
     */
    type get_type(const std::string& name, bool array)
    {
        auto it = std::find_if(type_map.begin(), type_map.end(),
                               [&name, &array](const std::pair<type, std::uint64_t>& t) -> bool
                               {
                                   if(name != t.first.get_base_type().s)
                                   {
                                       return false;
                                   }
                                   return (array && t.first.is_array())
                                          || (!array && !t.first.is_array());
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
                                       if(name != t.first.get_base_type().s)
                                       {
                                           return false;
                                       }
                                       if(t.first.is_array())
                                       {
                                           return false;
                                       }
                                       return true;
                                   });
            if(it != type_map.end())
            {
                // add the array type to the type map.
                auto type_id = generate_type_id();
                type_map.push_back({type{token{name, {0, 0}}, array, type_id, false}, type_id});
                return type_map.back().first;
            }
        }

        throw type_error(fmt::format("Unknown type '{}'.", name));
    }

    /**
     * Get a type object for an unresolved type and add mark the type for resolution.
     *
     * @param name The type name.
     * @param array Whether the type is an array type.
     * @param function_type Whether this type belongs to a function.
     * @returns An unresolved type.
     */
    type get_unresolved_type(token name, bool array, bool function_type)
    {
        auto it = std::find_if(unresolved_types.begin(), unresolved_types.end(),
                               [&name, &array, function_type](const type& t) -> bool
                               {
                                   if(name.s != t.get_base_type().s)
                                   {
                                       return false;
                                   }
                                   if(function_type != t.is_function_type())
                                   {
                                       return false;
                                   }
                                   return (array && t.is_array())
                                          || (!array && !t.is_array());
                               });
        if(it != unresolved_types.end())
        {
            return *it;
        }

        unresolved_types.push_back(type::make_unresolved(std::move(name), array, function_type));
        return unresolved_types.back();
    }

    /**
     * Return whether a type is convertible into another type.
     *
     * @note The checked types have to be different.
     *
     * @param from The type to convert from.
     * @param to The type to convert to.
     * @returns Whether the type `from` is convertible to the type `to`.
     */
    bool is_convertible(const type& from, const type& to) const;

    /**
     * Resolve all unresolved types.
     *
     * @throws Throws a `type_error` if type resolution failed.
     */
    void resolve_types();

    /**
     * The the type of a function.
     *
     * @param name The function's name.
     * @param arg_types The argument types.
     * @param ret_type The return type.
     * @returns The function type.
     */
    type get_function_type(const token& name, const std::vector<type>& arg_types, const type& ret_type);

    /**
     * Get the signature of a function.
     *
     * @throws A type_error if the function is not known.
     *
     * @param name The name of the function.
     * @returns A reference to the function signature.
     */
    const function_signature& get_function_signature(const token& name) const;

    /**
     * Add to the scope for name resolution.
     *
     * @param component The scope component to add.
     */
    void push_resolution_scope(std::string component);

    /**
     * Clear the last scope component for name resolution.
     *
     * @throws Throws a `type_error` if the scope stack was empty.
     */
    void pop_resolution_scope();

    /** Get the resolution scope. */
    std::string get_resolution_scope() const;

    /**
     * Enter a function's scope.
     *
     * @param name The function's name.
     */
    void enter_function_scope(token name);

    /**
     * Exit a function's scope.
     *
     * @param name The function's name. Used for validation.
     */
    void exit_function_scope(const token& name);

    /** Get the current function scope. */
    std::optional<function_signature> get_current_function() const;

    /** Enter an anonymous scope. */
    void enter_anonymous_scope(token_location loc);

    /** Exit an anonymous scope. */
    void exit_anonymous_scope();

    /** Get the current scope's name. */
    const token& get_scope_name() const;

    /**
     * Get the definition of a struct.
     *
     * @throws A type_error if the struct does not exist.
     *
     * @param loc A reference location for error reporting.
     * @param name The struct's name.
     * @returns A reference to the struct's definition.
     */
    const struct_definition* get_struct_definition(token_location loc, const std::string& name) const;

    /** Push a struct lookup. */
    void push_struct_definition(const struct_definition* s);

    /** Pop a struct definition. */
    void pop_struct_definition();

    /** Get the import list. */
    const std::vector<std::vector<token>>& get_imports() const
    {
        return imports;
    }

    /** Get a string representation of the context. */
    std::string to_string() const;
};

}    // namespace slang::typing