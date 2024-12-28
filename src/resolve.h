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
#include "module.h"
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

/** A module entry in the resolver's module list. */
struct module_entry
{
    /** Resolved import path of the module. */
    std::string resolved_path;

    /** Whether module resolution is completed. */
    bool is_resolved{false};
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
    file_manager& mgr;

    /** Loaded module headers, indexed by resolved import path. */
    std::unordered_map<std::string, module_::module_header> headers;

    /** List of modules. */
    std::vector<module_entry> modules;

    /** Function imports, indexed by resolved module path, as a pair `(name, descriptor)`. */
    std::unordered_map<
      std::string,
      std::vector<std::pair<std::string, module_::function_descriptor>>>
      imported_functions;

    /** Type imports, indexed by resolved module path, as a pair `(name, descriptor)`. */
    std::unordered_map<
      std::string,
      std::vector<std::pair<std::string, module_::struct_descriptor>>>
      imported_types;

    /** Constant imports, indexed by resolved module path, as a pair `(name, descriptor)`. */
    std::unordered_map<
      std::string,
      std::vector<std::pair<std::string, constant_descriptor>>>
      imported_constants;

protected:
    /**
     * Get a module header from a resolved path.
     *
     * @param resolved_path The resolved module path.
     * @returns Returns the module header, or throws a `file_error` if the module could not be opened.
     */
    module_::module_header& get_module_header(const fs::path& resolved_path);

    /**
     * Resolve imports for a given module.
     *
     * @param resolved_path The resolved module path.
     */
    void resolve_module(const fs::path& resolved_path);

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
    explicit context(file_manager& mgr)
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