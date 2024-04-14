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

#include <list>
#include <vector>
#include <string>
#include <unordered_map>

#include "token.h"

namespace slang::typing
{

/**
 * Check whether a string represents a built-in type, that is,
 * void, i32, f32 or str.
 */
inline bool is_builtin_type(const std::string& s)
{
    return s == "void" || s == "i32" || s == "f32" || s == "str";
}

/** Type errors. */
class type_error : public std::runtime_error
{
public:
    /**
     * Construct a type_error.
     *
     * NOTE Use the other constructor if you want to include location information in the error message.
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

    /** The variable's type. */
    token type;

    /** Default constructors. */
    variable_type() = default;
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
    variable_type(token name, token type)
    : name{std::move(name)}
    , type{std::move(type)}
    {
    }
};

/** A function signature. */
struct function_signature
{
    /** Name of the function. */
    token name;

    /** Argument types. */
    std::vector<token> arg_types;

    /** Return type. */
    token ret_type;

    /** Default constructors. */
    function_signature() = default;
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
     */
    function_signature(token name, std::vector<token> arg_types, token ret_type)
    : name{std::move(name)}
    , arg_types{std::move(arg_types)}
    , ret_type{std::move(ret_type)}
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

    /** The struct's members as (name, type) pairs. */
    std::vector<std::pair<token, token>> members;

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
    struct_definition(token name, std::vector<std::pair<token, token>> members)
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
     * @returns If the name is found, returns its type, and std::nullopt otherwise.
     */
    std::optional<std::string> get_type(const std::string& name) const
    {
        auto var_it = variables.find(name);
        if(var_it != variables.end())
        {
            return {var_it->second.type.s};
        }

        auto func_it = functions.find(name);
        if(func_it != functions.end())
        {
            return {func_it->second.to_string()};
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

    /** The current anonymous scope id. */
    std::size_t anonymous_scope_id = 0;

    /** Imported modules. */
    std::vector<std::vector<token>> imports;

    /** Struct/type stack, for member/type lookups. */
    std::vector<const struct_definition*> struct_stack;

public:
    /** Default constructor. */
    context() = default;
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
     * @param type String representation of the type.
     */
    void add_variable(token name, token type);

    /**
     * Add a function to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The function's name.
     * @param arg_types The functions's argument types.
     * @param ret_type The function's return type.
     */
    void add_function(token name, std::vector<token> arg_types, token ret_type);

    /**
     * Add a type to the context.
     *
     * @throws A type_error if the type already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The name of the type.
     * @param members The members, given as pairs (name, type).
     */
    void add_type(token name, std::vector<std::pair<token, token>> members);

    /**
     * Check if the context has a specific type.
     *
     * @param name The type's name.
     * @returns True if the type exists within the current scope.
     */
    bool has_type(const std::string& name) const;

    /**
     * Get the type for a name.
     *
     * @throws A type_error if the name is unknown.
     *
     * @param name The name.
     * @returns The string representation of the type.
     */
    std::string get_type(const token& name) const;

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