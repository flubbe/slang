/**
 * slang - a simple scripting language.
 *
 * module and import resolution.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <stdexcept>

#include "compiler/errors.h"
#include "shared/module.h"
#include "shared/token.h"
#include "filemanager.h"
#include "module_resolver.h"

namespace slang::loader
{

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
struct resolve_error : public source_error
{
    using source_error::source_error;
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
     * Get the module resolver for a given name.
     *
     * @param import_name The module's import name.
     * @returns A reference to the resolved module.
     * @throws Throws a `resolve_error` if the module does not have a resolver.
     */
    const module_::module_resolver& get_resolver(
      const std::string& import_name) const;

    /**
     * Resolve a module name.
     *
     * @param name The module's name.
     * @returns Returns the resolved name.
     */
    std::string resolve_name(const std::string& name) const;

    /**
     * Get available module names across search paths.
     *
     * @returns List of available module names.
     */
    std::vector<std::string> get_available_modules() const
    {
        return file_mgr.get_available_modules();
    }
};

}    // namespace slang::loader
