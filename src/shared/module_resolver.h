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
public:
    /**
     * Construct a `resolution_error`.
     *
     * @param message The error message.
     */
    explicit resolution_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** An recorder for module resolution information. */
struct resolution_recorder
{
    /** Default destructor. */
    virtual ~resolution_recorder() = default;

    /**
     * Begin recording a new section.
     *
     * @param name Name of the section.
     */
    virtual void section([[maybe_unused]] const std::string& name)
    {
    }

    /**
     * Record a constant table entry.
     *
     * @param c The constant.
     */
    virtual void constant([[maybe_unused]] const module_::constant_table_entry& c)
    {
    }

    /**
     * Record an exported symbol.
     *
     * @param s The exported symbol.
     */
    virtual void record([[maybe_unused]] const module_::exported_symbol& s)
    {
    }

    /**
     * Record an imported symbol.
     *
     * @param s The imported symbol.
     */
    virtual void record([[maybe_unused]] const module_::imported_symbol& s)
    {
    }
};

/** A module resolver for loading a module without decoding its bytecode. */
class module_resolver
{
protected:
    /** The module's path. */
    fs::path path;

    /** Module. */
    module_::language_module mod;

    /** A recorder for module information. */
    std::shared_ptr<resolution_recorder> recorder;

    /** Decode the module. */
    void decode();

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
     * @param recorder An optional recorder.
     */
    module_resolver(
      file_manager& file_mgr,
      fs::path path,
      std::shared_ptr<resolution_recorder> recorder = std::make_shared<resolution_recorder>());

    /** Get the module's path. */
    fs::path get_path() const
    {
        return path;
    }

    /** Get the module data. */
    const module_::language_module& get_module() const
    {
        return mod;
    }
};

}    // namespace slang::module_
