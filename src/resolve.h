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

#include "compiler/token.h"
#include "shared/module.h"
#include "filemanager.h"
#include "module_resolver.h"

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

namespace slang::interpreter
{
class module_loader;
}    // namespace slang::interpreter

namespace slang::resolve
{

/** A resolve error. */
class resolve_error : public std::runtime_error
{
public:
    /**
     * Construct a resolve_error.
     *
     * @note Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    explicit resolve_error(const std::string& message)
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

/** A constant descriptor. */
struct constant_descriptor
{
    /** The constant type. */
    module_::constant_type type;

    /** The constant's value. */
    std::variant<std::int32_t, float, std::string> value;
};

/** Resolver context. */
class context
{
    /** The associated file manager. */
    file_manager& file_mgr;

    /** Map of resolvers. */
    std::unordered_map<
      std::string,
      std::unique_ptr<module_::module_resolver>>
      resolvers;

protected:
    /**
     * Resolve imports for a given module. Only loads a module if it is not
     * already resolved.
     *
     * @param import_name The module's import name.
     * @returns A reference to the resolved module.
     */
    module_::module_resolver& resolve_module(const std::string& import_name);

    /**
     * Add a constant to the type- and code generation contexts.
     *
     * @param ctx Code generation context.
     * @param type_ctx Type context.
     * @param resolver Resolver for the module containing the constant.
     * @param import_path Import path of the module containing the constant.
     * @param name The constant's name.
     * @param index Index into the module's constant table.
     */
    void add_constant(
      codegen::context& ctx,
      typing::context& type_ctx,
      const module_::module_resolver& resolver,
      const std::string& import_path,
      const std::string& name,
      std::size_t index);

    /**
     * Add a function to the type- and code generation contexts.
     *
     * @param ctx Code generation context.
     * @param type_ctx Type context.
     * @param resolver Resolver for the module containing the function.
     * @param import_path Import path of the module containing the function.
     * @param name The function's name.
     * @param desc The function desciptor.
     */
    void add_function(
      codegen::context& ctx,
      typing::context& type_ctx,
      const module_::module_resolver& resolver,
      const std::string& import_path,
      const std::string& name,
      const module_::function_descriptor& desc);

    /**
     * Add a type to the type- and code generation contexts.
     *
     * @param ctx Code generation context.
     * @param type_ctx Type context.
     * @param resolver Resolver for the module containing the constant.
     * @param import_path Import path of the module containing the type.
     * @param name The type's name.
     * @param desc The type desciptor.
     */
    void add_type(
      codegen::context& ctx,
      typing::context& type_ctx,
      const module_::module_resolver& resolver,
      const std::string& import_path,
      const std::string& name,
      const module_::struct_descriptor& desc);

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
     * @param file_mgr The file manager used for path resolution.
     */
    explicit context(file_manager& file_mgr)
    : file_mgr{file_mgr}
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
