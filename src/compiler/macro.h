/**
 * slang - a simple scripting language.
 *
 * macro collection / expansion environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "shared/module.h"

namespace slang::macro
{

/**
 * A macro.
 *
 * FIXME Duplicated from codegen, should be removed there.
 */
class macro
{
    /** The macro name. */
    std::string name;

    /** The macro descriptor. */
    module_::macro_descriptor desc;

    /** Import path. */
    std::optional<std::string> import_path;

public:
    /** Constructors. */
    macro() = delete;
    macro(const macro&) = default;
    macro(macro&&) = default;

    /** Destructor. */
    ~macro() = default;

    /** Assignments. */
    macro& operator=(const macro&) = default;
    macro& operator=(macro&&) = default;

    /**
     * Create a new macro.
     *
     * @param name The macro's name.
     * @param desc Macro descriptor.
     * @param import_path Optional import path.
     */
    explicit macro(
      std::string name,
      module_::macro_descriptor desc,
      std::optional<std::string> import_path = std::nullopt)
    : name{std::move(name)}
    , desc{std::move(desc)}
    , import_path{std::move(import_path)}
    {
    }

    /** Get the macro's name. */
    [[nodiscard]]
    const std::string& get_name() const
    {
        return name;
    }

    /** Get the descriptor. */
    [[nodiscard]]
    const module_::macro_descriptor& get_desc() const
    {
        return desc;
    }

    /** Get the import path. */
    [[nodiscard]]
    const std::optional<std::string> get_import_path() const
    {
        return import_path;
    }

    /** Return whether this is an import. */
    [[nodiscard]]
    bool is_import() const
    {
        return import_path.has_value();
    }

    /** Whether this is a transitive import. */
    [[nodiscard]]
    bool is_transitive_import() const
    {
        return is_import() && name.starts_with("$");
    }

    /** Set transitivity. */
    void set_transitive(bool transitive)
    {
        bool transitive_name = name.starts_with("$");
        if(transitive_name && !transitive)
        {
            name = name.substr(1);
        }
        else if(!transitive_name && transitive)
        {
            name = std::string{"$"} + name;
        }
    }
};

/** Macro collection / expansion environment. */
struct env
{
    /**
     * List of macros.
     *
     * FIXME Duplicated from codegen context, should be removed there.
     */
    std::vector<std::unique_ptr<macro>> macros;

    /**
     * Add a macro definition.
     *
     * FIXME The wrong error type is thrown. Should be a variation of a redefinition error.
     *
     * @throws Throws a `std::runtime_error` if the macro already exists.
     * @param name The macro name.
     * @param desc The macro descriptor.
     * @param import_path Import path of the macro, or `std::nullopt`.
     */
    void add_macro(
      std::string name,
      module_::macro_descriptor desc,
      std::optional<std::string> import_path);
};

}    // namespace slang::macro
