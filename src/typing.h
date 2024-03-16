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

/** A function signature. */
struct function_signature
{
    /** Name of the function. */
    std::string name;

    /** Argument types. */
    std::vector<std::string> arg_types;

    /** Return type. */
    std::string ret_type;

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
    function_signature(std::string name, std::vector<std::string> arg_types, std::string ret_type)
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
    /** Variables (variables["name"] == "type"). */
    std::unordered_map<std::string, std::string> variables;

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
    std::vector<std::string> current_scopes = {"<global>"};

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
    void add_variable_type(std::string name, std::string type);

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
    void add_function_type(std::string name, std::vector<std::string> arg_types, std::string ret_type);

    /**
     * Get the type for a name.
     *
     * @throws A type_error if the name is unknown.
     *
     * @param name The name.
     */
    std::string get_type(const std::string& name) const;

    /**
     * Enter a named scope.
     *
     * @param name The scope's name.
     */
    void enter_named_scope(const std::string& name)
    {
        current_scopes.emplace_back(name);
    }

    /** Enter an anonymous scope. */
    void enter_anonymous_scope();

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