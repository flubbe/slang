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

#include <stdexcept>

#include "filemanager.h"
#include "token.h"

/*
 * Forward declarations.
 */
namespace slang::typing
{
class context;
}    // namespace slang::typing

namespace slang::codegen
{
class context;
}    // namespace slang::codegen

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
    /** The associated file manager. */
    file_manager& mgr;

public:
    /** Default constructors. */
    context() = delete;
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    /**
     * Construct a resolver context.
     *
     * @param mgr The file manager used for path resolution.
     */
    context(file_manager& mgr)
    : mgr{mgr}
    {
    }

    /**
     * Resolve imports from a type context.
     *
     * @param ctx The code generation context.
     * @param ctx The typing context.
     */
    void resolve_imports(slang::codegen::context& ctx, slang::typing::context& type_ctx);
};

}    // namespace slang::resolve