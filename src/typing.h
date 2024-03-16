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

#include <vector>
#include <string>
#include <unordered_map>

#include "token.h"

namespace slang::typing
{

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

/** A scope. */
struct scope
{
    /** Variables. */
    std::unordered_map<std::string, variable_type> variables;

    /** Functions. */
    std::unordered_map<std::string, function_signature> functions;

    /**
     * Check whether this scope contains a name.
     *
     * @param name The name to check for.
     * @returns True if the name exists in this scope or false if not.
     */
    bool contains(const std::string& name) const
    {
        return variables.find(name) != variables.end() || functions.find(name) != functions.end();
    }

    /** Get a string representation of the scope. */
    std::string to_string() const;
};

/** Type system context. */
class context
{
    /** Variables per scope. */
    std::unordered_map<std::string, scope> scopes;

    /** The current scopes. */
    std::vector<std::pair<std::optional<token_location>, std::string>> current_scopes = {{std::nullopt, "<global>"}};

    /** The current anonymous scope id. */
    std::size_t anonymous_scope_id = 0;

public:
    /** Default constructor. */
    context() = default;
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = default;
    context& operator=(context&&) = default;

    /**
     * Add a variable's type to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if the given type is unknown.
     *
     * @param name The variable's name.
     * @param type String representation of the type.
     */
    void add_variable_type(token name, token type);

    /**
     * Add a function's type to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The function's name.
     * @param arg_types The functions's argument types.
     * @param ret_type The function's return type.
     */
    void add_function_type(token name, std::vector<token> arg_types, token ret_type);

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
     */
    std::string get_type(const token& name) const;

    /**
     * Enter a named scope.
     *
     * @param loc The scope's enter location.
     * @param name The scope's name.
     */
    void enter_named_scope(token_location loc, std::string name)
    {
        current_scopes.emplace_back(std::make_pair<std::optional<token_location>, std::string>(std::move(loc), std::move(name)));
    }

    /** Enter an anonymous scope. */
    void enter_anonymous_scope(token_location loc);

    /** Exit an anonymous scope. */
    void exit_anonymous_scope();

    /**
     * Exit a scope.
     *
     * @param name The scope's name. Used for validation.
     */
    void exit_scope(const std::string& name);

    /** Get a string representation of the context. */
    std::string to_string() const;
};

}    // namespace slang::typing