/**
 * slang - a simple scripting language.
 *
 * module and import resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
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

namespace slang::macro
{
struct env;
}    // namespace slang::macro

namespace slang::loader
{

namespace cg = slang::codegen;
namespace ty = slang::typing;

/**
 * Generate an import name.
 *
 * @param name The name of the symbol.
 * @param transitive Whether this is a transitive import.
 * @returns Returns the import name.
 */
inline std::string make_import_name(
  const std::string& name,
  bool transitive)
{
    if(transitive)
    {
        return std::string("$") + name;
    }

    return name;
}

/** A resolve error. */
class resolve_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;

public:
    /**
     * Construct a resolve_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    resolve_error(const source_location& loc, const std::string& message);
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

public:
    /** Default constructors. */
    context() = delete;
    context(const context&) = default;
    context(context&&) = default;

    /** Default destructor. */
    ~context() = default;

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
     * Resolve imports for a given module. Only loads a module if it is not
     * already resolved.
     *
     * @param import_name The module's import name.
     * @param transitive Whether this is a transitive import, i.e. an import from a depencency resolution.
     * @returns A reference to the resolved module.
     */
    module_::module_resolver& resolve_module(
      const std::string& import_name,
      bool transitive);

    /**
     * Resolve macros.
     *
     * @note Macro resolution might lead to additional imports being needed. That is,
     *       if the function returns `true`, import resolution needs to be run.
     *
     * @param env Macro collection / expansion environment.
     * @param type_ctx The typing context.
     * @returns `true` if macros were resolved, and `false` otherwise.
     */
    static bool resolve_macros(
      macro::env& env,
      ty::context& type_ctx);
};

}    // namespace slang::loader
