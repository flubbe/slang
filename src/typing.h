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

/** Type system context. */
class context
{
    /** Variables per scope (variables["scope"]["name"] == "type"). */
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> variables;

    /** The current scopes. */
    std::vector<std::string> current_scopes;

public:
    /** Default constructor. */
    context() = default;
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = default;
    context& operator=(context&&) = default;

    /**
     * Add a variable to the context.
     *
     * @throws A
     *
     * @param name The variable's name.
     * @param type String representation of the variable's type.
     */
    void add_variable(std::string name, std::string type)
    {
        // TODO
    }

    /**
     * Enter a scope.
     *
     * @param name The scope's name.
     */
    void enter_scope(const std::string& name)
    {
        current_scopes.emplace_back(name);
    }

    /**
     * Exit a scope.
     */
    void exit_scope(const std::string& name);
};

}    // namespace slang::typing