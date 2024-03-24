/**
 * slang - a simple scripting language.
 *
 * name resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <exception>

/*
 * Forward declarations.
 */
namespace slang::typing
{
class context;
}    // namespace slang::typing

namespace slang::resolve
{

/** A resolve error. */
class resolve_error : public std::runtime_error
{
public:
    /**
     * Construct a resolve_error.
     *
     * NOTE Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    resolve_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a resolve_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    resolve_error(const token_location& loc, const std::string& message);
};

/** An import. */
struct import_definition
{
};

/** Resolver context. */
class context
{
public:
    /**
     * Resolve imports from a type context.
     *
     * @param ctx The type context.
     */
    void resolve_imports(slang::typing::context& ctx);
};

}    // namespace slang::resolve