/**
 * slang - a simple scripting language.
 *
 * module resolver.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "shared/module.h"
#include "filemanager.h"

namespace slang::module_
{

namespace fs = std::filesystem;

/** A module resolution error. */
class resolution_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/** A module resolver for loading a module without decoding its bytecode. */
class module_resolver
{
protected:
    /** The module's path. */
    fs::path path;

    /** Whether this is a transitive import. */
    bool transitive;

    /** Module. */
    module_::language_module mod;

public:
    /** Defaulted and deleted constructors. */
    module_resolver() = delete;
    module_resolver(const module_resolver&) = default;
    module_resolver(module_resolver&&) = default;

    /** Default assignments. */
    module_resolver& operator=(const module_resolver&) = delete;
    module_resolver& operator=(module_resolver&&) = delete;

    /**
     * Create a new module resolver.
     *
     * @param file_mgr The file manager to use.
     * @param path The module's path.
     * @param transitive Whether the module is a transitive import.
     */
    module_resolver(
      file_manager& file_mgr,
      fs::path path,
      bool transitive);

    /** Get the module's path. */
    fs::path get_path() const
    {
        return path;
    }

    /** Whether this module is a transitive import. */
    [[nodiscard]]
    bool is_transitive() const
    {
        return transitive;
    }

    /** Make the module an explicit import (instead of a transitive one). */
    void make_explicit()
    {
        transitive = false;
    }

    /** Get the module data. */
    [[nodiscard]]
    const module_::language_module& get_module() const
    {
        return mod;
    }
};

}    // namespace slang::module_
